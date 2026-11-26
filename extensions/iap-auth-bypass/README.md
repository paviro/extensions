# iAP Auth Bypass

Based on the RepriseOS core's [iAP auth bypass](https://github.com/RepriseOS/repriseos/blob/b9b1c62/payload/iap_auth.c), by RaduTek.

Lets accessories use authentication-gated iPod Accessory Protocol (iAP) lingos and commands without an Apple authentication coprocessor. Both signature verifiers report success, but accessories must still answer the certificate and signature requests of the handshake.

Disabling it restores signature verification for later handshakes; accessories already authenticated stay connected.

The handshake, the verification gate and the hook calling conventions are in the [iAP reversing notes](https://github.com/RepriseOS/repriseos/blob/b9b1c62/docs/iap-auth.md).

| | |
|---|---|
| Default | off |
| Memory | image only |
| Settings | none |
