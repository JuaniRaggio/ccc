# @see https://hub.docker.com/_/ubuntu
FROM ubuntu:25.04 AS runner

# Installs dependencies.
RUN apt-get update --yes \
	&& apt-get install --yes \
		bison \
		cmake \
		flex \
		gcc \
		iverilog \
		make \
		verilator \
	&& rm --force --recursive /var/lib/apt/lists/*
