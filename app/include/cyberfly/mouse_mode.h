#ifndef CYBERFLY_MOUSE_MODE_H
#define CYBERFLY_MOUSE_MODE_H

enum cyberfly_mouse_mode {
    CYBERFLY_MOUSE_OFF = 0,
    CYBERFLY_MOUSE_M1,   /* Kalman filter + state-space cursor (sensor fusion) */
    CYBERFLY_MOUSE_M2,   /* Accelerometer tilt-to-velocity (simple fallback) */
    CYBERFLY_MOUSE_MODE_COUNT,
};

#endif
