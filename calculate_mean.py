import datetime
import sys

path = "result/" + str(datetime.date.today())
filenames = [path + "/wifi_range_" + str(i + 1) + ".txt" for i in range (int(sys.argv[1]))]

def variance(lst):
    mean = float(sum(lst)) / len(lst)
    return float(reduce(lambda x, y: x + y, map(lambda x: (x - mean) ** 2, lst))) / len(lst)

def main():
    files = [open(file, "r") for file in filenames]
    for row in zip(*files):
        if '=' in row[0]:
            condition = ' '.join(row[0].split()[:-1])
            results = [float(result.split()[-1]) for result in row]
            print(condition + " " + str(sum(results) / len(results)) + " variance: " + str(variance(results)))
        else:
            print row[0].strip()

if __name__ == "__main__":
    main()