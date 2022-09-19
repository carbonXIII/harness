from scratch
ADD rootfs.tar.gz /

# Get build tools (cmake)
RUN echo "deb http://deb.debian.org/debian buster-backports main" >> /etc/apt/sources.list
RUN gpg --keyserver hkp://keyserver.ubuntu.com --recv-key 648ACFD622F3D138 \
    && gpg -a --export 648ACFD622F3D138 | sudo apt-key add -
RUN apt-get update --allow-releaseinfo-change
RUN apt-get install -y cmake/buster-backports
