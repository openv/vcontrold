# Vitola 20C8 profile

This profile is derived from the configuration tested on a device reporting
ID `20C8` (`VPlusHO1`, P300). The command definitions were read successfully
from that installation unless their description explicitly contains
`unbestaetigt` or `lokal unbestaetigt`.

The repository copy deliberately differs from the production runtime settings:
it listens on `127.0.0.1` and starts with `readonly=y`. Review the serial
device, listener, log path, service user/group and write policy before
deployment. Set commands remain defined so that an operator can enable them
deliberately; automated tests never execute them.

The reversible get/set/get checks covered:

- domestic hot-water target temperature;
- operating modes for heating circuits 1 and 2;
- reduced room targets for heating circuits 1 and 2;
- slope and level for heating curves 1 and 2.

System time has a matching set command but was intentionally excluded from
the reversible write tests.

Frost status addresses `2305` and `3305` are read-only status values and are
kept separate from operating-mode setters. Publicly suggested mixer position
addresses `254C` and `354C` are not part of the active profile: one-byte reads
returned P300 error code 1 on this controller, and the proposed two-byte
`PP` interpretation has not been confirmed locally. The confirmed mixer
information at `084C` is an actuator direction output (“M2 closing”), not a
continuous position.
