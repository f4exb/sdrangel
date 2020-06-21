FROM ubuntu:20.04 AS base
ARG uid

ENV DEBIAN_FRONTEND=noninteractive

# Create a user with sudo rights
RUN apt-get update && apt-get -y install sudo
RUN useradd -m appuser -u ${uid} && echo "appuser:appuser" | chpasswd \
   && adduser appuser sudo \
   && sudo usermod --shell /bin/bash appuser
RUN echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers
USER appuser

# Configure tzdata manually before anything else
ENV TZONE=Europe/Paris
RUN sudo ln -fs /usr/share/zoneinfo/$TZONE /etc/localtime \
    && sudo apt-get update && sudo apt-get -y install tzdata

# Base packages required
RUN sudo apt-get update && sudo apt-get -y install \
    git \
    vim \
    wget \
    maven \
    openjdk-8-jdk

# Prepare buiid and install environment
RUN sudo mkdir /opt/build /opt/install \
    && sudo chown appuser:appuser /opt/build /opt/install

# swagger-codegen
FROM base as codegen_clone
ARG clone_label
WORKDIR /opt/build
RUN git clone --depth 1 https://github.com/f4exb/swagger-codegen.git -b sdrangel \
    && cd swagger-codegen \
    && mkdir build \
    && echo "${clone_label}" > build/clone_label.txt

FROM base as codegen_build
COPY --from=codegen_clone --chown=appuser /opt/build/swagger-codegen /opt/build/swagger-codegen
WORKDIR /opt/build/swagger-codegen
RUN export JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64 \
    && mvn clean package
RUN mkdir -p /opt/install/swagger \
    && cp ./modules/swagger-codegen-cli/target/swagger-codegen-cli.jar /opt/install/swagger

FROM base as codegen
RUN mkdir -p /opt/build/sdrangel/swagger/sdrangel
COPY --from=codegen_build --chown=appuser /opt/install/swagger /opt/install/swagger
COPY swagger-codegen /opt/install/swagger

WORKDIR /opt/build/sdrangel/swagger/sdrangel
