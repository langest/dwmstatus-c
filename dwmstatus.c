#define _DEFAULT_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

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

int ReadFile(char* fileName, float* out) {
	FILE* file;
	file = fopen(fileName, "r");
	if (file == NULL) {
		DEBUG_PRINT("Failed to open file: %s\n", fileName);
		return 1;
	}
	fscanf(file, "%f", out);
	int err;
	err = fclose(file);
	if (err != 0) {
		DEBUG_PRINT("Failed to close file: %s\n", fileName);
		return 2;
	}
	return 0;
}

void GetScreenBrightness(char* out, const size_t size) {
	float max;
	int err = ReadFile("/sys/class/backlight/intel_backlight/max_brightness", &max);
	if (err != 0){
		snprintf(out, size, "err");
		return;
	}

	float now;
	err = ReadFile("/sys/class/backlight/intel_backlight/brightness", &now);
	if (err != 0) {
		snprintf(out, size, "err");
		return;
	}
	DEBUG_PRINT("Max brightness: %f\n", max);
	DEBUG_PRINT("Current brightness: %f\n", now);
	snprintf(out, size, "%0.f%%", now/max*100.0f);
}

void GetBatteryStatus(char* out, const size_t size) {
	float capacity;
	int err = ReadFile("/sys/class/power_supply/BAT0/energy_full", &capacity);
	if (err != 0) {
		snprintf(out, size, "err");
		return;
	}

	float energy;
	err = ReadFile("/sys/class/power_supply/BAT0/energy_now", &energy);
	if (err != 0) {
		snprintf(out, size, "err");
		return;
	}
	if (capacity < 0.0f || energy < 0.0f) {
		snprintf(out, size, "???%%");
		return;
	}

	/* Get battery status */
	const size_t statusSize = 12;
	char status[statusSize];
	FILE* file;
	file = fopen("/sys/class/power_supply/BAT0/status", "r");
	if (file == NULL) {
		DEBUG_ERROR("Failed to open battery status file\n");
		status[0] = '\0';
	} else {
		fscanf(file, "%s", status);
		DEBUG_PRINT("Battery status: %s\n", status);

		err = fclose(file);
		if (err != 0) {
			DEBUG_ERROR("Failed to close battery status file\n");
		}
		if (status[0] == 'C') { /* Charging */
			snprintf(status, statusSize, "%s", "⌁");
		} else { /* Discharging */
		status[0] = '\0';
		}
	}
	DEBUG_PRINT("Battery capacity: %f\n", capacity);
	DEBUG_PRINT("Battery energy: %f\n", energy);
	snprintf(out, size, "%s%0.f%%", status, energy/capacity*100.0f);
}

void GetAudioVolume(long* outvol) {
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

int GetTime(char* out, const size_t size) {
	time_t t;
	struct tm* localTime;

	time(&t);
	localTime = localtime(&t);
	if (localTime == NULL) {
		DEBUG_ERROR("Failed to get localTime\n");
		return 60;
	}

	int len = strftime(out, size, "W%W %a %d %b %H:%M", localTime);
	if (len <= 0) {
		DEBUG_ERROR("Failed to format time\n");
	}
	return localTime->tm_sec;
}

void GetKeyboardLayout(Display* display, char* out, const size_t size) {
	XkbDescRec* _kbdDescPtr = XkbAllocKeyboard();
	XkbGetNames(display, XkbSymbolsNameMask, _kbdDescPtr);
	Atom symName = _kbdDescPtr->names->symbols;
	char* layoutString = XGetAtomName(display, symName);

	/* Currently configured for US and SE */
	if (strstr(layoutString, "us")) {
		snprintf(out, size, "us");
	} else if (strstr(layoutString, "se")) {
		snprintf(out, size, "se");
	}
	XkbFreeKeyboard(_kbdDescPtr, XkbSymbolsNameMask, True);
}

void SetStatus(char* status, Display* display) {
	XStoreName(display, DefaultRootWindow(display), status);
	XSync(display, False);
}

void CatchSignal(int signo, siginfo_t* sinfo, void *context) {
	DEBUG_PRINT("Caught signal no: %d.\n", signo);
}

int main() {
	/* Register signal handler */
	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_sigaction = &CatchSignal;
	action.sa_flags = SA_SIGINFO;
	if (sigaction(SIGUSR1, &action, NULL) < 0) {
		DEBUG_ERROR("Failed to register signal handler\n");
		return 2;
	}

	Display* display;
	display = XOpenDisplay(NULL);
	if (display == NULL) {
		DEBUG_ERROR("Failed to open display\n");
		return 1;
	}

	const size_t brightnessMaxLength = 8;
	const size_t batteryMaxLength = 8;
	const size_t kbMaxLength = 4;
	const size_t timeMaxLength = 32;
	const size_t statusMaxLength = 128;
	char brightness[brightnessMaxLength];
	char battery[batteryMaxLength];
	char kb[kbMaxLength];
	long volume = -1;
	char time[timeMaxLength];
	char status[statusMaxLength];

	int sleepDuration = 0;
	while (True) {
		GetScreenBrightness(brightness, brightnessMaxLength);
		GetBatteryStatus(battery, batteryMaxLength);
		GetKeyboardLayout(display, kb, kbMaxLength);
		GetAudioVolume(&volume);

		sleepDuration = 60 - GetTime(time, timeMaxLength);

		snprintf(status, statusMaxLength, " ☼ %s ⋮ 🔋 %s ⋮ ⌨ %s ⋮ 🔊: %ld%% ⋮ %s",
		brightness, battery, kb, volume, time);
		SetStatus(status, display);
		DEBUG_PRINT("Set status to: %s, sleeping for %d seconds\n", status, sleepDuration);
		sleep(sleepDuration);
	}

	XCloseDisplay(display);

	return 0;
}
