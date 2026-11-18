//
// main.cpp - Bare metal MSX for Raspberry Pi 3
//
#include "kernel.h"
#include <circle/startup.h>

int main (void)
{
	CKernel Kernel;
	if (!Kernel.Initialize ())
	{
		halt ();
		return EXIT_HALT;
	}

	CStdlibApp::TShutdownMode ShutdownMode = Kernel.Run ();

	switch (ShutdownMode)
	{
	case CStdlibApp::ShutdownReboot:
		reboot ();
		return EXIT_REBOOT;

	case CStdlibApp::ShutdownHalt:
	default:
		halt ();
		return EXIT_HALT;
	}
}
