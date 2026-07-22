#!/usr/bin/env bash
# Publish a Let's Encrypt certificate into a directory the sidecar container can
# actually read, and bounce the sidecar so it picks the new one up.
#
#   sudo deploy/install-certs.sh game.cowengine.com          # one-time + renewals
#
# Why this exists: certbot keeps /etc/letsencrypt/{live,archive} at 0700 root:root,
# and the sidecar runs as an unprivileged user, so bind-mounting that tree gives
# "Permission denied" and the sidecar silently falls back to plain ws://. Rather
# than loosen /etc/letsencrypt or run the container as root, copy the two PEMs to
# $TLS_DIR owned by the container's uid. Copying also dodges the fact that
# live/<domain>/*.pem are relative symlinks into ../../archive/.
#
# Install it as a certbot deploy hook so renewals stay automatic:
#   sudo cp deploy/install-certs.sh /etc/letsencrypt/renewal-hooks/deploy/cowengine.sh
#   sudo chmod +x /etc/letsencrypt/renewal-hooks/deploy/cowengine.sh
# certbot sets RENEWED_LINEAGE when it runs hooks, so no argument is needed there.
set -euo pipefail

TLS_DIR="${TLS_DIR:-/opt/cowengine/tls}"
COMPOSE_DIR="${COMPOSE_DIR:-/opt/cowengine}"
COW_UID="${COW_UID:-10001}"

if [[ $EUID -ne 0 ]]; then
    echo "install-certs: must run as root (it reads /etc/letsencrypt)" >&2
    exit 1
fi

# certbot exports RENEWED_LINEAGE=/etc/letsencrypt/live/<domain> for deploy hooks.
lineage="${RENEWED_LINEAGE:-}"
if [[ -z "$lineage" ]]; then
    domain="${1:-}"
    [[ -n "$domain" ]] || { echo "usage: sudo install-certs.sh <domain>" >&2; exit 1; }
    lineage="/etc/letsencrypt/live/$domain"
fi
[[ -f "$lineage/fullchain.pem" ]] || { echo "install-certs: no cert at $lineage" >&2; exit 1; }

# Set ownership by raw uid. COW_UID names a user that exists only *inside* the
# container, so anything that goes through a name lookup can fail on the host
# ("install: invalid user: '10001'" — seen on Ubuntu). The leading '+' tells
# coreutils to skip the lookup and take the number literally; try it as a
# fallback, since not every chown accepts that form.
chown_to_uid() {
    chown "$COW_UID:$COW_UID" "$1" 2>/dev/null \
        || chown "+$COW_UID:+$COW_UID" "$1"
}

install -d -m 0755 "$TLS_DIR"
# `install` copies content, so the live/ symlinks are resolved here.
install -m 0444 "$lineage/fullchain.pem" "$TLS_DIR/fullchain.pem"
install -m 0400 "$lineage/privkey.pem"   "$TLS_DIR/privkey.pem"
chown_to_uid "$TLS_DIR/fullchain.pem"
chown_to_uid "$TLS_DIR/privkey.pem"
echo "install-certs: published $lineage -> $TLS_DIR (owner $COW_UID)"

# Reload the sidecar: it reads the PEMs once, at startup. `restart` (not `up -d`)
# is deliberate — the PEM paths don't change on renewal, so `up -d` sees identical
# config and does nothing at all, leaving the old cert loaded. Conversely, if you
# ever edit TLS_DIR in .env, `restart` keeps the OLD bind mount and you need
# `up -d` to recreate the container.
#
# Never fail the whole script here — as a certbot deploy hook that would report
# the *renewal* as failed, when in fact the cert is published and only the reload
# was missed.
if [[ ! -f "$COMPOSE_DIR/docker-compose.prod.yml" ]]; then
    echo "install-certs: no stack at $COMPOSE_DIR yet — restart the sidecar once it's up" >&2
elif (cd "$COMPOSE_DIR" && docker compose -f docker-compose.prod.yml restart sidecar); then
    echo "install-certs: sidecar restarted"
else
    echo "install-certs: sidecar restart FAILED — the cert is published, reload it manually" >&2
fi
