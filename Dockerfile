FROM debian:bookworm-slim

RUN apt-get update -qq && \
    apt-get install -y -qq clang llvm lld bsdmainutils make git && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src

CMD ["bash", "-c", "clang tests/test_c64_shortcuts.c -o test_shortcuts && ./test_shortcuts && cd src/bios && make clean && make"]
