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
#include <QBoxLayout>
#include <QLayout>
#include <QLayoutItem>
#include <QMainWindow>
#include <QDockWidget>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QSlider>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <QWidget>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static bool hide_panel = true;
static bool toolbar_dock = false;
static bool status_dock = true;
static bool shrink_docks = true;
static QAction *toggle_action = nullptr;

static void load_settings()
{
	char *path = obs_module_config_path("settings.json");
	if (!path)
		return;

	obs_data_t *data = obs_data_create_from_json_file(path);
	if (data) {
		obs_data_set_default_bool(data, "hide_panel", true);
		obs_data_set_default_bool(data, "toolbar_dock", false);
		obs_data_set_default_bool(data, "status_dock", true);
		obs_data_set_default_bool(data, "shrink_docks", true);
		hide_panel = obs_data_get_bool(data, "hide_panel");
		toolbar_dock = obs_data_get_bool(data, "toolbar_dock");
		status_dock = obs_data_get_bool(data, "status_dock");
		shrink_docks = obs_data_get_bool(data, "shrink_docks");
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
	obs_data_set_bool(data, "toolbar_dock", toolbar_dock);
	obs_data_set_bool(data, "status_dock", status_dock);
	obs_data_set_bool(data, "shrink_docks", shrink_docks);
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

	/* Let the Preview and Program areas take all of the freed-up width. */
	QWidget *editor = panel->parentWidget();
	auto *layout = editor ? qobject_cast<QBoxLayout *>(editor->layout()) : nullptr;
	if (!layout) {
		obs_log(LOG_WARNING, "preview layout not found");
		return;
	}

	for (int i = 0; i < layout->count(); i++) {
		QWidget *w = layout->itemAt(i)->widget();
		if (!w || w == panel)
			continue;
		layout->setStretchFactor(w, 1);
		layout->setAlignment(w, Qt::Alignment());
		w->setMaximumWidth(QWIDGETSIZE_MAX);
	}
	layout->invalidate();
	layout->activate();
}

/* Source toolbar ("contextContainer") as its own dock. Must run in obs_module_load so
 * OBS can restore the dock position together with the other docks. */
static void create_toolbar_dock()
{
	auto *main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	QWidget *toolbar = main_window ? main_window->findChild<QWidget *>(QStringLiteral("contextContainer"))
				       : nullptr;
	if (!toolbar) {
		obs_log(LOG_WARNING, "source toolbar not found");
		return;
	}

	const bool was_visible = !toolbar->isHidden();
	auto *container = new QWidget();
	auto *layout = new QVBoxLayout(container);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(toolbar);
	layout->addStretch(1);
	toolbar->setVisible(was_visible);

	if (!obs_frontend_add_dock_by_id("hstp_source_toolbar", obs_module_text("ToolbarDockTitle"), container))
		obs_log(LOG_WARNING, "could not add source toolbar dock");
	else
		obs_log(LOG_INFO, "source toolbar moved into its own dock");
}

/* Wrap a dock's contents in a scroll area so the dock can be made shorter than
 * its contents; a scroll bar appears instead of the dock refusing to shrink. */
static void make_dock_shrinkable(QMainWindow *main_window, const char *name)
{
	auto *dock = main_window->findChild<QDockWidget *>(QString::fromUtf8(name));
	QWidget *content = dock ? dock->widget() : nullptr;
	if (!content || qobject_cast<QScrollArea *>(content))
		return;

	auto *scroll = new QScrollArea();
	scroll->setObjectName(QStringLiteral("hstpScroll"));
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidgetResizable(true);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	scroll->setStyleSheet(QStringLiteral("QScrollArea#hstpScroll, QScrollArea#hstpScroll > QWidget > QWidget "
					     "{ background: transparent; border: none; }"));
	scroll->setWidget(content);
	dock->setWidget(scroll);
	obs_log(LOG_INFO, "dock '%s' can now shrink", name);
}

static void apply_shrinkable_docks()
{
	auto *main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main_window)
		return;
	make_dock_shrinkable(main_window, "mixerDock");
	make_dock_shrinkable(main_window, "controlsDock");
}

/* Compact status dock: LIVE / REC elapsed time, CPU and FPS. It reads the
 * same information as the OBS status bar but does not touch the status bar. */
static uint64_t stream_start_ns = 0;
static uint64_t rec_start_ns = 0;
static uint64_t rec_paused_ns = 0;
static uint64_t rec_pause_begin_ns = 0;
static os_cpu_usage_info_t *cpu_info = nullptr;
static QLabel *live_label = nullptr;
static QLabel *rec_label = nullptr;
static QLabel *perf_label = nullptr;

static QString format_elapsed(uint64_t ns)
{
	const uint64_t total = ns / 1000000000ULL;
	return QStringLiteral("%1:%2:%3")
		.arg(total / 3600, 2, 10, QLatin1Char('0'))
		.arg((total / 60) % 60, 2, 10, QLatin1Char('0'))
		.arg(total % 60, 2, 10, QLatin1Char('0'));
}

static void set_state(QLabel *label, const QString &text, const char *color)
{
	label->setText(text);
	label->setStyleSheet(QStringLiteral("QLabel { color: %1; font-weight: bold; }").arg(QString::fromUtf8(color)));
}

static void update_status_dock()
{
	if (!live_label)
		return;
	const uint64_t now = os_gettime_ns();

	if (obs_frontend_streaming_active() && stream_start_ns)
		set_state(live_label, QStringLiteral("● LIVE ") + format_elapsed(now - stream_start_ns), "#ff4b4b");
	else
		set_state(live_label, QStringLiteral("○ LIVE --:--:--"), "#7a7a7a");

	if (obs_frontend_recording_active() && rec_start_ns) {
		uint64_t paused = rec_paused_ns;
		if (obs_frontend_recording_paused() && rec_pause_begin_ns)
			paused += now - rec_pause_begin_ns;
		const QString time = format_elapsed(now - rec_start_ns - paused);
		if (obs_frontend_recording_paused())
			set_state(rec_label, QStringLiteral("❚❚ REC ") + time, "#f0b429");
		else
			set_state(rec_label, QStringLiteral("● REC ") + time, "#ff4b4b");
	} else {
		set_state(rec_label, QStringLiteral("○ REC --:--:--"), "#7a7a7a");
	}

	const double cpu = cpu_info ? os_cpu_usage_info_query(cpu_info) : 0.0;
	perf_label->setText(QStringLiteral("CPU %1%   %2 FPS")
				    .arg(cpu, 0, 'f', 1)
				    .arg(obs_get_active_fps(), 0, 'f', 2));
}

static void create_status_dock()
{
	cpu_info = os_cpu_usage_info_start();

	auto *container = new QWidget();
	auto *layout = new QHBoxLayout(container);
	layout->setContentsMargins(8, 4, 8, 4);
	layout->setSpacing(16);
	live_label = new QLabel(container);
	rec_label = new QLabel(container);
	perf_label = new QLabel(container);
	layout->addWidget(live_label);
	layout->addWidget(rec_label);
	layout->addStretch(1);
	layout->addWidget(perf_label);

	auto *timer = new QTimer(container);
	QObject::connect(timer, &QTimer::timeout, []() { update_status_dock(); });
	timer->start(500);
	update_status_dock();

	if (!obs_frontend_add_dock_by_id("hstp_status", obs_module_text("StatusDockTitle"), container))
		obs_log(LOG_WARNING, "could not add status dock");
}

static void track_outputs(enum obs_frontend_event event)
{
	const uint64_t now = os_gettime_ns();
	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		stream_start_ns = now;
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		stream_start_ns = 0;
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		rec_start_ns = now;
		rec_paused_ns = 0;
		rec_pause_begin_ns = 0;
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		rec_start_ns = 0;
		break;
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
		rec_pause_begin_ns = now;
		break;
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
		if (rec_pause_begin_ns)
			rec_paused_ns += now - rec_pause_begin_ns;
		rec_pause_begin_ns = 0;
		break;
	default:
		break;
	}
}

static void schedule_apply()
{
	/* Run after OBS has finished building the studio mode layout. */
	QTimer::singleShot(0, []() { apply_panel_visibility(); });
}

static void frontend_event(enum obs_frontend_event event, void *private_data)
{
	UNUSED_PARAMETER(private_data);

	track_outputs(event);

	switch (event) {
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
		schedule_apply();
		break;
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		schedule_apply();
		if (shrink_docks)
			QTimer::singleShot(0, []() { apply_shrinkable_docks(); });
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

	auto add_restart_option = [](const char *text_key, bool *value) {
		auto *action = static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(obs_module_text(text_key)));
		if (!action)
			return;
		action->setCheckable(true);
		action->setChecked(*value);
		QObject::connect(action, &QAction::toggled, [value](bool checked) {
			*value = checked;
			save_settings();
		});
	};
	add_restart_option("ToolbarDock", &toolbar_dock);
	add_restart_option("ShrinkDocks", &shrink_docks);
	add_restart_option("StatusDock", &status_dock);

	if (toolbar_dock)
		create_toolbar_dock();
	if (status_dock)
		create_status_dock();

	obs_frontend_add_event_callback(frontend_event, nullptr);

	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	if (cpu_info) {
		os_cpu_usage_info_destroy(cpu_info);
		cpu_info = nullptr;
	}
	obs_log(LOG_INFO, "plugin unloaded");
}
