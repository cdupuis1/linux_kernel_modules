#!/bin/sh

# Remove previous key
rm -f *.priv
rm -f *.x509

# Generate new key
openssl req -new -x509 -newkey rsa:2048 -keyout module_signing_key.priv -outform DER -out module_signing_key.x509 -nodes -days 36500 -subj "/CN=Custom Module Signing Key/"

# Sign module using key created by openssl
/usr/src/linux-headers-$(uname -r)/scripts/sign-file sha256 module_signing_key.priv module_signing_key.x509 blk_example.ko
