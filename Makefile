all:clean
	sudo ./mk_client.sh 
	sudo ./mk_server.sh
clean:
	rm -rf ssl_server ssl_client

