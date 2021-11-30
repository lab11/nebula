## To compile

make clean
make -j

## To run

* Server A (leader/primary): ubuntu@34.205.45.52

	`./server -leader -dataSize 128 -leaderIP 34.205.45.52:4442 -followerIP 18.209.20.193:4443 -numRows 0 -numThreads 36`

* Server B (follower/secondary): ubuntu@18.209.20.193

	`./server -dataSize 128 -leaderIP 34.205.45.52:4442 -followerIP 18.209.20.193:4443 -numRows 0 -numThreads 36``

* Client

	`./client -dataSize 128 -leaderIP 34.205.45.52:4442 -followerIP 18.209.20.193:4443 -numExistingRows 0 -numThreads 52``
