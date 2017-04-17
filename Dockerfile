FROM alpine
RUN apk add --no-cache libc-dev gcc g++ ncurses-dev make openssh tini
COPY . /src
WORKDIR /src
RUN make
RUN adduser -s /src/bike -S bike && \
    passwd -u bike && \
    echo PermitEmptyPasswords yes >> /etc/ssh/sshd_config && \
    echo ForceCommand /src/bike >> /etc/ssh/sshd_config && \
    ssh-keygen -A
CMD ["/sbin/tini", "/usr/sbin/sshd", "-D"]
EXPOSE 22
