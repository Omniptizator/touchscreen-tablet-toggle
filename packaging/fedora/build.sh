#!/usr/bin/env bash
# Run from source root in a Fedora build environment.
set -euo pipefail
spec=packaging/fedora/touchscreen-tablet-toggle.spec
nv=$(rpmspec -q --srpm --qf '%{name}-%{version}' "$spec")
top=$(mktemp -d)
trap 'rm -rf "$top"' EXIT
out=dist/native/fedora
mkdir -p "$top/SOURCES" "$out"
tar --sort=name --mtime="@${SOURCE_DATE_EPOCH:-0}" --owner=0 --group=0 \
    --exclude=.git --exclude=dist --exclude=build --exclude=__pycache__ --exclude='*.py' \
    --transform="s,^\.,$nv," -czf "$top/SOURCES/$nv.tar.gz" .
rpmbuild --define "_topdir $top" -ba "$spec"
cp "$top"/RPMS/*/"$nv"-*.rpm "$top"/SRPMS/*.rpm "$out/"
(cd "$out" && sha256sum "$nv"-*.rpm > SHA256SUMS)
