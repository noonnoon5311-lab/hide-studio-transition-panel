/*
Hide Studio Mode Transition Panel
Hides the transition controls shown between Preview and Program in OBS Studio Mode,
so that the Preview and Program displays can use the freed-up space.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <plugin-support.h>

#include <QAction>
#include <QMainWindow>
#include <QSlider>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <QWidget>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static bool hide_panel = true;
static QAction *toggle_action = nullptr;

static void load_settings()
{
	char *path = obs_module_config_path("settings.json");
	if (!path)
		return;

	obs_data_t *data = obs_data_create_from_json_file(path);
	if (data) {
		obs_data_set_default_bool(data, "hide_panel", true);
		hide_panel = obs_data_get_bool(data, "hide_panel");
		obs_data_release(data);
	}
	bfree(path);
}

static void save_settings()
{
	char *dir = obs_module_config_path("");
	if (dir) {
		os_mkdirs(dir);
		bfree(dir);
	}

	char *path = obs_module_config_path("settings.json");
	if (!path)
		return;

	obs_data_t *data = obs_data_create();
	obs_data_set_bool(data, "hide_panel", hide_panel);
	obs_data_save_json_safe(data, path, "tmp", "bak");
	obs_data_release(data);
	bfree(path);
}

/* The transition panel (OBS calls it "programOptions") has no object name, but it
 * is the parent widget of the T-Bar slider, which carries the "slider-tbar" class. */
static QWidget *find_transition_panel()
{
	auto *main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main_window)
		return nullptr;

	const auto sliders = main_window->findChildren<QSlider *>();
	for (QSlider *slider : sliders) {
		if (slider->property("class").toString() == QStringLiteral("slider-tbar"))
			return slider->parentWidget();
	}
	return nullptr;
}

static void apply_panel_visibility()
{
	if (!obs_frontend_preview_program_mode_active())
		return;

	QWidget *panel = find_transition_panel();
	if (!panel) {
		obs_log(LOG_WARNING, "transition panel not found");
		return;
	}

	panel->setVisible(!hide_panel);
	obs_log(LOG_INFO, "transition panel %s", hide_panel ? "hidden" : "shown");
}

static void schedule_apply()
{
	/* Run after OBS has finished building the studio mode layout. */
	QTimer::singleShot(0, []() { apply_panel_visibility(); });
}

static void frontend_event(enum obs_frontend_event event, void *private_data)
{
	UNUSED_PARAMETER(private_data);

	switch (event) {
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		schedule_apply();
		break;
	default:
		break;
	}
}

bool obs_module_load(void)
{
	load_settings();

	toggle_action = static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(obs_module_text("HidePanel")));
	if (toggle_action) {
		toggle_action->setCheckable(true);
		toggle_action->setChecked(hide_panel);
		QObject::connect(toggle_action, &QAction::toggled, [](bool checked) {
			hide_panel = checked;
			save_settings();
			apply_panel_visibility();
		});
	}

	obs_frontend_add_event_callback(frontend_event, nullptr);

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	obs_log(LOG_INFO, "plugin unloaded");
}
