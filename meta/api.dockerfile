FROM scratch

ADD lib64 /lib64
ADD lib /lib

ADD KEGE /bin/
CMD ["/lib64/ld-linux-x86-64.so.2", "/bin/KEGE"]
