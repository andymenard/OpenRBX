# Writes a build timestamp to STAMP. The client compares it with the copy in its data directory to
# decide whether the packaged runtime files need to be installed again.
string(TIMESTAMP now "%Y%m%d%H%M%S" UTC)
file(WRITE "${STAMP}" "${now}\n")
