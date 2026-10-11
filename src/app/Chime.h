// Chime.h - the notification sound for received messages.
//
// The Python version called QApplication.beep(), which modern Windows
// plays as nothing at all.  This is a short two-tone chime synthesised at
// start-up and played from memory (PlaySound), so there is no sound file
// to ship and no multimedia module to deploy.
#pragma once

namespace chime {
void play();
}
