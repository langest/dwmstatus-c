#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

#include <alsa/asoundlib.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#if defined(DEBUG) && DEBUG > 0
#define DEBUG_PRINT(fmt, ...) fprintf(stderr, "[DEBUG] %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define DEBUG_ERROR(fmt) fprintf(stderr, "[ERROR] %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__)
#else
#define DEBUG_PRINT(fmt, ...)
#define DEBUG_ERROR(fmt)
#endif

/* Blocking ping */
/* Blocking shouldn't be a problem
 * since the rest of the program is
 * waiting for almost a minute. So
 * there is plenty of time to await
 * a response
 */
int try_ping(const char* ip_address, const char* timeout) {
	if(fork() == 0) { //child
		execl("/bin/ping", "ping", "-w", timeout, "-c1", ip_address, "&>", "/dev/null", (char*)NULL);
	}
	int status;
	wait(&status);
	return status == 0;
}

	const char* ip = "xxx.xxx.xxx.xxx"; // Any ip that you can only reach when VPN is up
void get_vpn_status(char* out, size_t size, const char* timeout) {

	if (try_ping(ip, timeout) == 1) {
		DEBUG_ERROR("Could reach ip, VPN appears to be up\n");
		snprintf(out, size, "yes");
		return;
	}
	DEBUG_ERROR("Could NOT reach ip, VPN appears to be down\n");
	snprintf(out, size, "no");
}

int read_file(char* file, float* out) {
	FILE* f;
	f = fopen(file, "r");
	if (f == NULL) {
		DEBUG_PRINT("Failed to open file: %s\n", file);
		return 1;
	}
	int err = fscanf(f, "%f", out);
	if (err < 0) {
		DEBUG_PRINT("Failed to scan file: %s\n", file);
		return 2;
	}
	err = fclose(f);
	if (err != 0) {
		DEBUG_PRINT("Failed to close file: %s\n", file);
		return 2;
	}
	return 0;
}

void get_screen_brightness(char* out, const size_t size) {
	float max;
	int err = read_file("/sys/class/backlight/intel_backlight/max_brightness", &max);
	if (err != 0){
		snprintf(out, size, "err");
		return;
	}

	float now;
	err = read_file("/sys/class/backlight/intel_backlight/actual_brightness", &now);
	if (err != 0) {
		snprintf(out, size, "err");
		return;
	}
	DEBUG_PRINT("Max brightness: %f\n", max);
	DEBUG_PRINT("Current brightness: %f\n", now);
	snprintf(out, size, "%0.f%%", now/max*100.0f);
}

void get_battery_status(int battery_index, char* out, const size_t size) {
	int capacity;
	FILE* file;
	int path_max_length = 40;
	char info_file[path_max_length];
	snprintf(info_file, path_max_length, "/sys/class/power_supply/BAT%d/capacity", battery_index);

	file = fopen(info_file, "r");
	if (file) {
		int err = fscanf(file, "%d", &capacity);
		if (err < 0) {
			DEBUG_ERROR("Failed to scan battery capacity file\n");
			snprintf(out, size, "err");
			return;
		}
		err = fclose(file);
		if (err != 0) {
			DEBUG_ERROR("Failed to close battery capacity file\n");
			snprintf(out, size, "err");
			return;
		}
	} else {
		DEBUG_ERROR("Did not find battery capacity, assuming no battery present\n");
		snprintf(out, size, "missing");
		return;
	}

	if (capacity < 0) {
		snprintf(out, size, "???%%");
		return;
	}

	/* Get battery status */
	const size_t status_size = 16;
	char status[status_size];
	snprintf(info_file, path_max_length, "/sys/class/power_supply/BAT%d/status", battery_index);
	file = fopen(info_file, "r");
	if (file == NULL) {
		DEBUG_ERROR("Failed to open battery status file\n");
		status[0] = '\0';
	} else {
		int err = fscanf(file, "%s", status);
		if (err != 0) {
			DEBUG_ERROR("Failed to scan battery status file\n");
		}
		DEBUG_PRINT("Battery status: %s\n", status);

		err = fclose(file);
		if (err != 0) {
			DEBUG_ERROR("Failed to close battery status file\n");
		}
		if (status[0] == 'C') { /* Charging */
			snprintf(status, status_size, "%s", "⌁⏶");
		} else if (status[0] == 'D'){ /* Discharging */
			snprintf(status, status_size, "%s", "⌁⏷");
		} else { /* Uknown / not used */
			status[0] = '\0';
		}
	}
	DEBUG_PRINT("Battery capacity: %d\n", capacity);
	snprintf(out, size, "%s%d%%", status, capacity);
}

void get_audio_volume(long* outvol) {
	*outvol = -1;
	snd_mixer_t* handle;
	snd_mixer_elem_t* elem;
	snd_mixer_selem_id_t* sid;

	static const char* mix_name = "Master";
	static const char* card = "default";
	static int mix_index = 0;

	snd_mixer_selem_id_alloca(&sid);

	snd_mixer_selem_id_set_index(sid, mix_index);
	snd_mixer_selem_id_set_name(sid, mix_name);

	if ((snd_mixer_open(&handle, 0)) < 0) {
		DEBUG_ERROR("Failed to open mixer");
		return;
	}
	if ((snd_mixer_attach(handle, card)) < 0) {
		snd_mixer_close(handle);
		DEBUG_ERROR("Failed to attach handle");
		return;
	}
	if ((snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
		snd_mixer_close(handle);
		DEBUG_ERROR("Failed to register handle");
		return;
	}
	int ret = snd_mixer_load(handle);
	if (ret < 0) {
		snd_mixer_close(handle);
		DEBUG_ERROR("Failed to load handle");
		return;
	}
	elem = snd_mixer_find_selem(handle, sid);
	if (!elem) {
		snd_mixer_close(handle);
		DEBUG_ERROR("Failed to find elem");
		return;
	}

	long minv;
	long maxv;
	snd_mixer_selem_get_playback_volume_range(elem, &minv, &maxv);
	DEBUG_PRINT("Volume range <%ld,%ld>\n", minv, maxv);

	if(snd_mixer_selem_get_playback_volume(elem, 0, outvol) < 0) {
		snd_mixer_close(handle);
		DEBUG_ERROR("Failed to get playback volume");
		return;
	}

	DEBUG_PRINT("Get volume %ld with status %i\n", *outvol, ret);
	*outvol -= minv;
	maxv -= minv;
	minv = 0;
	*outvol = 100 * (*outvol) / maxv;

	snd_mixer_close(handle);
}

int get_time(char* out, const size_t size) {
	time_t t;
	struct tm* local_time;

	time(&t);
	local_time = localtime(&t);
	if (local_time == NULL) {
		DEBUG_ERROR("Failed to get local_time\n");
		return 60;
	}

	char time[size];
	int len = strftime(time, size, "W%W %%s %d %b %H:%M", local_time);
	if (len <= 0) {
		DEBUG_ERROR("Failed to format time\n");
	}
	static const char* weekdays = "日\0月\0火\0水\0木\0金\0土\0";
	snprintf(out, size, time, &weekdays[local_time->tm_wday*4]);
	return local_time->tm_sec;
}

void get_keyboard_layout(Display* display, char* out, const size_t size) {
	XkbDescRec* _kbdDescPtr = XkbAllocKeyboard();
	XkbGetNames(display, XkbSymbolsNameMask, _kbdDescPtr);
	Atom sym_name = _kbdDescPtr->names->symbols;
	char* layout_string = XGetAtomName(display, sym_name);

	/* Currently configured for US and SE */
	if (strstr(layout_string, "us")) {
		snprintf(out, size, "us");
	} else if (strstr(layout_string, "se")) {
		snprintf(out, size, "se");
	}
	XkbFreeKeyboard(_kbdDescPtr, XkbSymbolsNameMask, True);
}

void set_status(char* status, Display* display) {
	XStoreName(display, DefaultRootWindow(display), status);
	XSync(display, False);
}

void catch_signal(int signo, siginfo_t* sinfo, void *context) {
	DEBUG_PRINT("Caught signal no: %d.\n", signo);
}

void catch_signal_and_sleep(int signo, siginfo_t* sinfo, void *context) {
	DEBUG_PRINT("Caught signal no: %d.\n", signo);
	nanosleep((const struct timespec[]){{0, 160000000L}}, NULL);
}

int main() {
	/* Register signal handler */
	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_sigaction = &catch_signal;
	action.sa_flags = SA_SIGINFO;
	if (sigaction(SIGUSR1, &action, NULL) < 0) {
		DEBUG_ERROR("Failed to register signal handler\n");
		return 2;
	}
	action.sa_sigaction = &catch_signal_and_sleep;
	if (sigaction(SIGUSR2, &action, NULL) < 0) {
		DEBUG_ERROR("Failed to register signal handler\n");
		return 2;
	}

	Display* display;
	display = XOpenDisplay(NULL);
	if (display == NULL) {
		DEBUG_ERROR("Failed to open display\n");
		return 1;
	}

	const size_t brightness_max_length = 8;
	const size_t battery_max_length = 16;
	const size_t keyboard_max_length = 4;
	const size_t vpn_max_length = 8;
	const size_t time_max_length = 32;
	const size_t status_max_length = 128;
	const char* vpn_timeout = "10";
	char brightness[brightness_max_length];
	char battery0[battery_max_length];
	char battery1[battery_max_length];
	char kb[keyboard_max_length];
	long volume = -1;
	char vpn[vpn_max_length];
	char time[time_max_length];
	char status[status_max_length];

	int sleep_duration;
	while (True) {
		get_screen_brightness(brightness, brightness_max_length);
		get_battery_status(0, battery0, battery_max_length);
		get_battery_status(1, battery1, battery_max_length);
		get_keyboard_layout(display, kb, keyboard_max_length);
		get_audio_volume(&volume);
		get_vpn_status(vpn, vpn_max_length, vpn_timeout);

		sleep_duration = 60 - get_time(time, time_max_length);

		snprintf(status, status_max_length, " bklight: %s | bat: %s, %s | ⌨ %s | 🔊 %ld%% | vpn %s | %s",
		brightness,
		battery0,
		battery1,
		kb,
		volume,
		vpn,
		time);

		set_status(status, display);
		DEBUG_PRINT("Set status to: %s, sleeping for %d seconds\n", status, sleep_duration);
		sleep(sleep_duration);
	}

	XCloseDisplay(display);

	return 0;
}
