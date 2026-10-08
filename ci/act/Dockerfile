# Local runner image for nektos/act (see docs/LOCAL_CI.md).
#
# act's stock runner image has the GitHub Actions JavaScript runtime but not
# the build tools GitHub's hosted ubuntu-latest ships with. The unit-test and
# integration jobs configure with cmake -S tests and package with rsync, so add
# only those, keeping everything else identical to the stock image.
FROM ghcr.io/catthehacker/ubuntu:act-latest

RUN apt-get update \
    && apt-get install -y --no-install-recommends cmake ninja-build rsync \
    && rm -rf /var/lib/apt/lists/*
