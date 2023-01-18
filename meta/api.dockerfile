FROM scratch

ADD . /
CMD ["/lib64/ld-linux-x86-64.so.2", "/KEGE"]
