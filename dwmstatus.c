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

void GetBatteryStatus(char* out, const size_t size) {
	FILE* file;
	/* Check battery present */
	file = fopen("/sys/class/power_supply/BAT0/present", "r");
	if (file == NULL) {
		DEBUG_ERROR("Failed to open battery present file\n");
		snprintf(out, size, "Bat: err");
		return;
	}
	const int present = fgetc(file);

	int err;
	err = fclose(file);
	if (err != 0) {
		DEBUG_ERROR("Failed to close battery present file\n");
		snprintf(out, size, "Bat: err");
		return;
	}
	if (present == 0) {
		snprintf(out, size, "Bat: nope");
		return;
	}

	/* Get battery capacity */
	file = fopen("/sys/class/power_supply/BAT0/energy_full", "r");
	if (file == NULL) {
		DEBUG_ERROR("Failed to open battery capacity file\n");
		snprintf(out, size, "Bat: err");
		return;
	}
	int capacity = -1;
	fscanf(file, "%d", &capacity);

	err = fclose(file);
	if (err != 0) {
		DEBUG_ERROR("Failed to close battery capacity file\n");
		snprintf(out, size, "Bat: err");
		return;
	}

	/* Get battery energy */
	file = fopen("/sys/class/power_supply/BAT0/energy_now", "r");
	if (file == NULL) {
		DEBUG_ERROR("Failed to open battery energy file\n");
		snprintf(out, size, "Bat: err");
		return;
	}
	int energy = -1;
	fscanf(file, "%d", &energy);

	err = fclose(file);
	if (err != 0) {
		DEBUG_ERROR("Failed to close battery energy file\n");
		snprintf(out, size, "Bat: err");
		return;
	}

	if (capacity == -1 || energy == -1) {
		snprintf(out, size, "Bat: ???%%");
		return;
	}

	/* Get battery status */
	char status;
	char statusFile[12];
	file = fopen("/sys/class/power_supply/BAT0/status", "r");
	if (file == NULL) {
		DEBUG_ERROR("Failed to open battery status file\n");
		status = '?';
	} else {
		fscanf(file, "%s", statusFile);
		DEBUG_PRINT("Battery status: %s\n", statusFile);

		err = fclose(file);
		if (err != 0) {
			DEBUG_ERROR("Failed to close battery status file\n");
		}
		if (strstr(statusFile, "Di")) {
			status = '-';
		} else if (strstr(statusFile, "Ch")) {
			status = '+';
		}
	}
	DEBUG_PRINT("Battery capacity: %d\n", capacity);
	DEBUG_PRINT("Battery energy: %d\n", energy);
	snprintf(out, size, "Bat: %c %d%%", status, energy/capacity*100);
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

	int len = strftime(out, size, "W%W %a %d %b %H:%M:%S", localTime);
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

	const size_t batteryMaxLength = 16;
	const size_t kbMaxLength = 4;
	const size_t timeMaxLength = 32;
	const size_t statusMaxLength = 128;
	char battery[batteryMaxLength];
	char kb[kbMaxLength];
	long volume = -1;
	char time[timeMaxLength];
	char status[statusMaxLength];

	int sleepDuration = 0;
	while (True) {
		GetBatteryStatus(battery, batteryMaxLength);
		GetKeyboardLayout(display, kb, kbMaxLength);
		GetAudioVolume(&volume);

		sleepDuration = 60 - GetTime(time, timeMaxLength);

		snprintf(status, statusMaxLength, " %s   KB: %s   Vol: %ld   %s", battery, kb, volume, time);
		SetStatus(status, display);
		DEBUG_PRINT("Set status to: %s, sleeping for %d seconds\n", status, sleepDuration);
		sleep(sleepDuration);
	}

	XCloseDisplay(display);

	return 0;
}
