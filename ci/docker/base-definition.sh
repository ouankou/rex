#!/bin/sh
# Print the content identity of the REX base image definition.  Base images
# are published under this identity so that an image build uses the base
# produced from the same definition it checked out, never a stale one.
set -eu
cd "$(dirname "$0")"
sha256sum rex-base.Dockerfile | cut -c1-16
