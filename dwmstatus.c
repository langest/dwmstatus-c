#include <stdio.h>

#include <X11/Xlib.h>

#if defined(DEBUG) && DEBUG > 0
#define DEBUG_PRINT(fmt, ...) fprintf(stderr, "[DEBUG] %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define DEBUG_ERROR(fmt) fprintf(stderr, "[ERROR] %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__)
#else
#define DEBUG_PRINT(fmt, ...)
#define DEBUG_ERROR(fmt)
#endif

void SetStatus(char* status, Display* display) {
	XStoreName(display, DefaultRootWindow(display), status);
	XSync(display, False);
}

int main() {
	Display* display;
	display = XOpenDisplay(NULL);
	if (display == NULL) {
		DEBUG_ERROR("Failed to open display");
		return 1;
	}

	char status[128];
	sprintf(status, "My status");
	SetStatus(status, display);

	XCloseDisplay(display);

	return 0;
}
