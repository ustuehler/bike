FROM alpine
RUN apk add --no-cache libc-dev gcc g++ ncurses-dev make
COPY . /src
WORKDIR /src
RUN make
CMD ["./bike"]
