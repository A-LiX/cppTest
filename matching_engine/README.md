# Matching Engine

This project contains two types of matching engine implementations: normal matching (`normal_matching`) and auction matching (`auction_matching`).

## Build

In the project root directory, run:

```bash
make
```

This will generate two executables:
- `normal_matching`
- `auction_matching`

## Run

After building, you can run the executables directly. For example:

```bash
./normal_matching
./auction_matching
```

## Test

Replace the root directory csv file with your testing file, make sure that your testing file has the same name as 'orders.csv'.

## Clean

To remove compiled files, run:

```bash
make clean
```



