FROM ubuntu:16.04@sha256:1f1a2d56de1d604801a9671f301190704c25d604a416f59e03c04f5c6ffee0d6

ENV DEBIAN_FRONTEND=noninteractive

# A imagem e os repositórios Xenial preservam as versões do ambiente validado.
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      bison \
      build-essential \
      flex \
      git \
      gsl-bin \
      g++-multilib \
      libgsl2 \
      libqt4-dev \
      python \
      qt4-dev-tools \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace/IEEE-802.11ah-ns-3
CMD ["/bin/bash"]
