cmd_/home/fly/fly/drives/demo/smodule.mod := printf '%s\n'   smodule.o | awk '!x[$$0]++ { print("/home/fly/fly/drives/demo/"$$0) }' > /home/fly/fly/drives/demo/smodule.mod
