cmd_/home/fly/fly/drives/hello/virhello.mod := printf '%s\n'   virhello.o | awk '!x[$$0]++ { print("/home/fly/fly/drives/hello/"$$0) }' > /home/fly/fly/drives/hello/virhello.mod
