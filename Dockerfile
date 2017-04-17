FROM alpine
RUN apk add --no-cache libc-dev gcc g++ ncurses-dev make openssh tini
COPY . /src
WORKDIR /src
RUN make
RUN adduser -s /src/bike -S bike && \
    passwd -u bike && \
    echo ForceCommand /src/bike >> /etc/ssh/sshd_config && \
    echo PermitEmptyPasswords yes >> /etc/ssh/sshd_config && \
    echo X11Forwarding no >> /etc/ssh/sshd_config && \
    echo AllowTcpForwarding no >> /etc/ssh/sshd_config && \
    echo AllowAgentForwarding no >> /etc/ssh/sshd_config && \
    ssh-keygen -A
CMD ["/sbin/tini", "/usr/sbin/sshd", "-D"]
EXPOSE 22
