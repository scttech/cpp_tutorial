# Sandbox image: compiles and runs ONE untrusted job, then is destroyed.
# Hardening (no network, read-only rootfs, dropped caps, limits) is applied
# by the runner-service when it creates the container, not baked in here.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      clang-18 lld-18 libc++-18-dev libc++abi-18-dev libclang-rt-18-dev llvm-18 g++-14 python3 \
 && ln -s /usr/bin/clang++-18 /usr/local/bin/clang++ \
 && ln -s /usr/bin/clang-18 /usr/local/bin/clang \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --uid 10001 --no-create-home --shell /usr/sbin/nologin sandbox

COPY zth-run.py /opt/zth/zth-run.py
COPY include/ /opt/zth/include/

USER 10001:10001
WORKDIR /work
ENTRYPOINT ["python3", "/opt/zth/zth-run.py"]
