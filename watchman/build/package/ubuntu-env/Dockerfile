ARG UBUNTU_VERSION
FROM ubuntu:$UBUNTU_VERSION

# https://serverfault.com/a/1016972 to ensure installing tzdata does
# not result in a prompt that hangs forever.
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

RUN apt-get -y update
RUN apt-get -y install python3 gcc g++ libssl-dev curl sudo

RUN curl https://sh.rustup.rs -sSf | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"

# Avoid build output from Watchman's build getting buffered in large
# chunks, which makes debugging progress tough.
ENV PYTHONUNBUFFERED=1
