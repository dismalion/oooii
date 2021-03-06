// Copyright (c) 2014 Antony Arciuolo. See License.txt regarding use.
#include <oConcurrency/backoff.h>
#include <oCore/filesystem.h>
#include <oCore/reporting.h>
#include <oGUI/msgbox.h>
#include <oGUI/msgbox_reporting.h>
#include <oGUI/Windows/win_gdi_bitmap.h>
#include <oGUI/Windows/win_common_dialog.h>
#include <oGUI/menu.h>
#include <oGUI/enum_radio_handler.h>
#include <oGUI/window.h>
#include "resource.h"

#include "../about_ouroboros.h"

#include <oGfx/core.h>
#include <oGfx/render_window.h>
#include <oGfx/surface_view.h>
#include <oGPU/all.h>
#include <oSurface/codec.h>

using namespace ouro;
using namespace ouro::gui;
using namespace windows::gdi;

static const char* sAppName = "oTexView";

// status bar
enum oWSTATUSBAR
{
	oWSTATUSBAR_INFO,
};

enum oWMENU
{
	oWMENU_FILE,
	oWMENU_EDIT,
	oWMENU_VIEW,
	oWMENU_VIEW_ZOOM,
	oWMENU_HELP,
	oWMENU_COUNT,
	oWMENU_TOPLEVEL,
};

struct oWMENU_HIER
{
	oWMENU Parent; // use oWMENU_TOPLEVEL for root parent menu
	oWMENU Menu;
	const char* Name;
};

static oWMENU_HIER sMenuHier[] = 
{
	{ oWMENU_TOPLEVEL, oWMENU_FILE, "&File" },
	{ oWMENU_TOPLEVEL, oWMENU_EDIT, "&Edit" },
	{ oWMENU_TOPLEVEL, oWMENU_VIEW, "&View" },
	{ oWMENU_VIEW, oWMENU_VIEW_ZOOM, "&Zoom" },
	{ oWMENU_TOPLEVEL, oWMENU_HELP, "&Help" },
};
static_assert(oCOUNTOF(sMenuHier) == oWMENU_COUNT, "array mismatch");

enum oWHOTKEY
{
	oWHK_FILE_OPEN,
	oWHK_VIEW_ZOOM_QUARTER,
	oWHK_VIEW_ZOOM_HALF,
	oWHK_VIEW_ZOOM_ORIGINAL,
	oWHK_VIEW_ZOOM_DOUBLE,
};

enum oWMI // menuitems
{
	oWMI_FILE_OPEN,
	oWMI_FILE_EXIT,

	oWMI_VIEW_ZOOM_QUARTER,
	oWMI_VIEW_ZOOM_HALF,
	oWMI_VIEW_ZOOM_ORIGINAL,
	oWMI_VIEW_ZOOM_DOUBLE,
	
	oWMI_VIEW_ZOOM_FIRST = oWMI_VIEW_ZOOM_QUARTER,
	oWMI_VIEW_ZOOM_LAST = oWMI_VIEW_ZOOM_DOUBLE,

	oWMI_HELP_ABOUT,
};

basic_hotkey_info HotKeys[] =
{
	// reset style
	{ input::o, oWHK_FILE_OPEN, false, true, false },
	{ input::_1, oWHK_VIEW_ZOOM_QUARTER, true, false, false },
	{ input::_2, oWHK_VIEW_ZOOM_HALF, true, false, false },
	{ input::_3, oWHK_VIEW_ZOOM_ORIGINAL, true, false, false },
	{ input::_4, oWHK_VIEW_ZOOM_DOUBLE, true, false, false },
};


class oTexViewApp
{
public:
	oTexViewApp();
	~oTexViewApp();

	void Run();

private:
	std::shared_ptr<window> AppWindow;
	window* pGPUWindow;

	gfx::core gfxcore;
	gfx::render_window gpuwin;
	gfx::surface_view sv;

	// @tony: this is a bit lame. Everything is currently using the immediate context
	// which cannot be accessed from multiple threads even with a multi-threaded device.
	// so assign data to this loaded value and kick something to consume it.
	surface::info info_from_file; // might've required conversion
	surface::image displayed;

	menu_handle Menus[oWMENU_COUNT];
	menu::enum_radio_handler EnumRadioHandler;
	window_state::value PreFullscreenState;
	bool Running;
	bool zoom_enabled;
private:
	void on_action(const input::action& a);
	void on_event(const window::basic_event& e);
	void on_drop(const window::basic_event& e);
	void on_zoom(int id);
	void CreateMenus(const window::create_event& e);
	void open_file_dialog();
	void open_file(const path& p);
};

oTexViewApp::oTexViewApp()
	: pGPUWindow(nullptr)
	, Running(true)
	, zoom_enabled(true)
	, PreFullscreenState(window_state::hidden)
{
	// Set up application window
	{
		window::init i;
		i.title = sAppName;
		i.icon = (icon_handle)load_icon(IDI_APPICON);
		i.on_action = std::bind(&oTexViewApp::on_action, this, std::placeholders::_1);
		i.on_event = std::bind(&oTexViewApp::on_event, this, std::placeholders::_1);
		i.shape.client_size = int2(256, 256);
		i.shape.state = window_state::hidden;
		i.shape.style = window_style::fixed_with_menu_and_statusbar;
		i.alt_f4_closes = true;
		i.cursor_state = cursor_state::arrow;
		AppWindow = window::make(i);
		AppWindow->set_hotkeys(HotKeys);

		const int sSections[] = { 300, -1 };
		AppWindow->set_num_status_sections(sSections, oCOUNTOF(sSections));
		AppWindow->set_status_text(oWSTATUSBAR_INFO, "Texture Info");
	}

	// Initialize render resources
	gfxcore.initialize("gpu window thread", true);
	
	sv.initialize(gfxcore);

	// Set up child window so the app can have Win32 GUI and GPU rendering
	{
		gfx::render_window_init i(sAppName);
		i.icon = (icon_handle)load_icon(IDI_APPICON);
		i.hotkeys = HotKeys;
		i.num_hotkeys = oCOUNTOF(HotKeys);
		i.color_format = surface::format::r8g8b8a8_unorm;
		i.depth_format = surface::format::d24_unorm_s8_uint;
		i.on_action = std::bind(&oTexViewApp::on_action, this, std::placeholders::_1);
		i.on_stop = [&] { Running = false; };
		i.render = std::bind(&gfx::surface_view::render, &sv);
		i.parent = AppWindow;

		pGPUWindow = gpuwin.start(gfxcore.device, i);
		pGPUWindow->hook_events(std::bind(&oTexViewApp::on_drop, this, std::placeholders::_1));
	}

	// attach render app to window rendering
	auto& ct = gpuwin.color_target();
	sv.set_draw_target(&ct);
	AppWindow->show();
}

oTexViewApp::~oTexViewApp()
{
	filesystem::join();
}

void oTexViewApp::CreateMenus(const window::create_event& _CreateEvent)
{
	for (auto& m : Menus)
		m = menu::make_menu();

	for (const auto& h : sMenuHier)
	{
		menu::append_submenu(
			h.Parent == oWMENU_TOPLEVEL ? _CreateEvent.menu : Menus[h.Parent]
		, Menus[h.Menu], h.Name);
	}

	// File menu
	menu::append_item(Menus[oWMENU_FILE], oWMI_FILE_OPEN, "&Open...\tCtrl+O");
	menu::append_item(Menus[oWMENU_FILE], oWMI_FILE_EXIT, "E&xit\tAlt+F4");

	// Edit menu
	// (nothing yet)

	// View menu

	const char* sZoomMenu[] = 
	{
		"1:4 Quarter\tAlt+1",
		"1:2 Half\tAlt+2",
		"1:1 Original\tAlt+3",
		"2:1 Double\tAlt+4",
	};
	static_assert(oCOUNTOF(sZoomMenu) == (oWMI_VIEW_ZOOM_LAST-oWMI_VIEW_ZOOM_FIRST+1), "array mismatch");

	for (int i = oWMI_VIEW_ZOOM_FIRST; i <= oWMI_VIEW_ZOOM_LAST; i++)
	{
		menu::append_item(Menus[oWMENU_VIEW_ZOOM], i, sZoomMenu[i - oWMI_VIEW_ZOOM_FIRST]);
		menu::enable(Menus[oWMENU_VIEW_ZOOM], i, false);
	}

	zoom_enabled = false;

	// Help menu
	menu::append_item(Menus[oWMENU_HELP], oWMI_HELP_ABOUT, "About...");
}

void oTexViewApp::on_event(const window::basic_event& e)
{
	switch (e.type)
	{
		case event_type::activated:
			oTRACE("event_type::activated");
			break;

		case event_type::deactivated:
			oTRACE("event_type::deactivated");
			break;

		case event_type::creating:
		{
			oTRACE("event_type::creating");
			CreateMenus(e.as_create());
			break;
		}

		case event_type::sized:
		{
			oTRACE("event_type::sized %s %dx%d", as_string(e.as_shape().shape.state), e.as_shape().shape.client_size.x, e.as_shape().shape.client_size.y);
			if (pGPUWindow)
				pGPUWindow->client_size(e.as_shape().shape.client_size);
			break;
		}

		case event_type::closing:
			gpuwin.stop();
			break;

		case event_type::drop_files:
			on_drop(e);
			break;

		default:
			break;
	}
}

void oTexViewApp::on_drop(const window::basic_event& e)
{
	if (e.type != event_type::drop_files)
		return;

	const int n = e.as_drop().num_paths;
	const path_string* paths = e.as_drop().paths;

	if (n >= 1)
		open_file(path(paths[0]));
}

void oTexViewApp::on_zoom(int item)
{
	if (!zoom_enabled || item < oWMI_VIEW_ZOOM_FIRST || item > oWMI_VIEW_ZOOM_LAST)
		return;
	menu::check_radio(Menus[oWMENU_VIEW_ZOOM], oWMI_VIEW_ZOOM_FIRST, oWMI_VIEW_ZOOM_LAST, item);

	int2 NewSize = info_from_file.dimensions.xy();
	switch (item)
	{
		case oWMI_VIEW_ZOOM_QUARTER: NewSize /= 4; break;
		case oWMI_VIEW_ZOOM_HALF: NewSize /= 2; break;
		case oWMI_VIEW_ZOOM_DOUBLE: NewSize *= 2; break;
		default: break;
	}

	auto Center = AppWindow->client_position() + (AppWindow->client_size() / 2);
	auto NewCenter = AppWindow->client_position() + (NewSize / 2);
	auto diff = NewCenter - Center;

	AppWindow->client_position(AppWindow->client_position() - diff);

	AppWindow->client_size(NewSize);
}

void oTexViewApp::on_action(const input::action& _Action)
{
	switch (_Action.action_type)
	{
		case input::action_type::menu:
		{
			switch (_Action.device_id)
			{
				case oWMI_FILE_OPEN:
					open_file_dialog();
					break;

				case oWMI_FILE_EXIT:
					gpuwin.stop();
					break;

				case oWMI_VIEW_ZOOM_QUARTER:
				case oWMI_VIEW_ZOOM_HALF:
				case oWMI_VIEW_ZOOM_ORIGINAL:
				case oWMI_VIEW_ZOOM_DOUBLE:
					on_zoom(_Action.device_id);
					break;

				case oWMI_HELP_ABOUT:
				{
					oDECLARE_ABOUT_INFO(i, load_icon(IDI_APPICON));
					std::shared_ptr<about> a = about::make(i);
					a->show_modal(AppWindow);
					break;
				}
				default:
					EnumRadioHandler.on_action(_Action);
					break;
			}
			break;
		}

		case input::action_type::hotkey:
		{
			switch (_Action.device_id)
			{
				case oWHK_FILE_OPEN:
					open_file_dialog();
					break;

				case oWHK_VIEW_ZOOM_QUARTER:
				case oWHK_VIEW_ZOOM_HALF:
				case oWHK_VIEW_ZOOM_ORIGINAL:
				case oWHK_VIEW_ZOOM_DOUBLE:
					// @tony: eww... is there a way to make all hotkeys trigger menu events?
					on_zoom(_Action.device_id - oWHK_VIEW_ZOOM_QUARTER + oWMI_VIEW_ZOOM_QUARTER);
					break;

				default:
					oTRACE("input::hotkey");
					break;
			}
			
			break;
		}

		default:
			break;
	}
}

void oTexViewApp::open_file(const path& p)
{
	if (surface::file_format::unknown == surface::get_file_format(p))
	{
		msgbox(msg_type::info, AppWindow->native_handle(), sAppName, "Unsupported file type %s", p.c_str());
		return;
	}

	try
	{
		{
			auto decoded = surface::decode(filesystem::load(p));
			info_from_file = decoded.get_info();

			if (is_texture(info_from_file.format))
				displayed = std::move(decoded);
			else
			{
				auto converted = decoded.convert(surface::format::b8g8r8a8_unorm);
				displayed = std::move(converted);
			}
		}
		
		pGPUWindow->dispatch([=]
		{ 
			try
			{
				sv.set_texels("displayed", displayed);
				AppWindow->set_title("%s - %s", sAppName, p.c_str());
				AppWindow->set_status_text(oWSTATUSBAR_INFO, "%u x %u %s", info_from_file.dimensions.x, info_from_file.dimensions.y, as_string(info_from_file.format));

				// resize based on actual data
				auto inf = displayed.get_info();

				{
					for (int i = oWMI_VIEW_ZOOM_FIRST; i <= oWMI_VIEW_ZOOM_LAST; i++)
						menu::enable(Menus[oWMENU_VIEW_ZOOM], i);
					menu::check_radio(Menus[oWMENU_VIEW_ZOOM], oWMI_VIEW_ZOOM_FIRST, oWMI_VIEW_ZOOM_LAST, oWMI_VIEW_ZOOM_ORIGINAL);
					zoom_enabled = true;
				}

				AppWindow->dispatch([=] {on_zoom(oWMI_VIEW_ZOOM_ORIGINAL); });
			}

			catch (std::exception& e)
			{
				msgbox(msg_type::info, AppWindow->native_handle(), sAppName, "Opening %s failed:\n%s", p.c_str(), e.what());
			}

			displayed.deinitialize();
		});
	}

	catch (std::exception& e)
	{
		msgbox(msg_type::info, AppWindow->native_handle(), sAppName, "Opening %s failed:\n%s", p.c_str(), e.what());
	}
}

void oTexViewApp::open_file_dialog()
{
	const char* sSupportedFormats = 
		"Supported Image Types|*.bmp;*.dds;*.jpg;*.png;*.psd;*.tga" \
		"|All Files Types|*.*"\
		"|Bitmap Files|*.bmp" \
		"|DDS Files|*.dds" \
		"|JPG/JPEG Files|*.jpg" \
		"|PNG Files|*.png" \
		"|Photoshop Files|*.psd" \
		"|Targa Files|*.tga";

	path p(filesystem::data_path());
	if (!windows::common_dialog::open_path(p, "Open Texture", sSupportedFormats, (HWND)AppWindow->native_handle()))
		return;

	open_file(p);
}

void oTexViewApp::Run()
{
KeepGoing:
	try
	{
		while (Running)
			AppWindow->flush_messages();
	}
	catch (std::exception& e)
	{
		msgbox(msg_type::info, nullptr, "oTexViewApp", "ERROR\n%s", e.what());
		goto KeepGoing;
	}
}

int main(int argc, const char* argv[])
{
	reporting::set_prompter(prompt_msgbox);

	oTexViewApp App;
	App.Run();
	return 0;
}
