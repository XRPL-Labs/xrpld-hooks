# Use the official image as a parent image.
FROM centos

# Set the working directory.
WORKDIR /opt/xrpld-hooks/

# Copy the file from your host to your current location.
COPY docker/screenrc /root/.screenrc
COPY docker/wasm2wat /usr/bin/
COPY rippled .
COPY testnet.cfg .
COPY testnetvalidators.txt .
COPY docker/libboost/libboost_coroutine.so.1.70.0 /usr/lib/
COPY docker/libboost/libboost_context.so.1.70.0 /usr/lib
COPY docker/libboost/libboost_filesystem.so.1.70.0 /usr/lib
COPY docker/libboost/libboost_program_options.so.1.70.0 /usr/lib
COPY docker/libboost/libboost_regex.so.1.70.0 /usr/lib
COPY docker/libboost/libboost_system.so.1.70.0 /usr/lib
COPY docker/libboost/libboost_thread.so.1.70.0 /usr/lib
COPY docker/libboost/libboost_chrono.so.1.70.0 /usr/lib
COPY docker/libboost/libboost_date_time.so.1.70.0 /usr/lib
COPY docker/libboost/libboost_atomic.so.1.70.0 /usr/lib
COPY docker/js/ ./
# Run the command inside your image filesystem.
RUN dnf install epel-release -y
RUN yum install -y vim screen python3-setuptools-wheel python3-pip-wheel python3 python3-pip curl make nodejs
RUN curl https://cmake.org/files/v3.17/cmake-3.17.1-Linux-x86_64.sh --output cmake-3.17.1-Linux-x86_64.sh \
    &&  mkdir /opt/cmake \
    &&  printf "y\nn\n" | sh cmake-3.17.1-Linux-x86_64.sh --prefix=/opt/cmake > /dev/null \
    &&  ln -s /opt/cmake/bin/cmake /usr/local/bin/cmake
RUN curl https://raw.githubusercontent.com/wasienv/wasienv/master/install.sh | sh
RUN echo 'PATH=$PATH:/root/.wasienv/bin/' >> /root/.bash_rc
RUN rm -f cmake-3.17.1-Linux-x86_64.sh
RUN mkdir /etc/opt/ripple
RUN ln -s /opt/xrpld-hooks/testnet.cfg /etc/opt/ripple/rippled.cfg
RUN ln -s /opt/xrpld-hooks/testnetvalidators.txt /etc/opt/ripple/testnetvalidators.txt

# Add metadata to the image to describe which port the container is listening on at runtime.
EXPOSE 6005
EXPOSE 5005

# Run the specified command within the container.
CMD ./rippled --conf testnet.cfg --net >> log 2>> log
