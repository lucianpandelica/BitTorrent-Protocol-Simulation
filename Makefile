build:
	mpic++ -o tema3 tema3.cpp client.cpp tracker.cpp helpers.cpp -pthread -Wall

clean:
	rm -rf tema3
