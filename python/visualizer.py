
import os
import matplotlib.pyplot as plt
from matplotlib.ticker import StrMethodFormatter


def getValues(pFile):
    """Obtains the list results of a Results.txt file

    Parameters:
    ----
    pFile: File
        Results.txt file of a server
    """
    values = []
    pFile.seek(0)
    for line in pFile:
        values.append(line.strip("\n").split(","))

    return values



def createPlot(pFiles, pServers, pFig, pPos, pXIdx, pYIdx, 
                    pYAxisName, pXAxisName, pTitle):
    """Create a new subplot

    Parameters:
    ----
    pFiles: List
        List of Results.txt files of each server
    pServers: List
        List of names of each server
    pFig: matplotlib.plot.figure
        Figure where the plots are drawn
    pPos: int
        Position of the subplot
    pXIdx, pYIdx: int
        Index of the X and Y values inside the list of results
    pYAxisName, pXAxisName: str
        Lable of the Y and X axis
    pTitle: str
        Title of the subplot
    """
    legends = []
    axis = pFig.add_subplot(pPos)
    for i in range(len(pFiles)):
        values = getValues(pFiles[i])
        if len(values) != 0:
            legends.append(pServers[i])
            xValues = []
            yValues = []
            for j in range(len(values)):
                xValues.append(values[j][pXIdx])
                yValues.append(float(values[j][pYIdx]))
            plt.plot(xValues, yValues, linestyle='--', marker='o')

    axis.set_ylabel(pYAxisName)
    axis.set_xlabel(pXAxisName)
    axis.set_title(pTitle)
    plt.gca().yaxis.set_major_formatter(StrMethodFormatter('{x:,.3f}'))
    plt.legend(legends)
    pFig.tight_layout()

    
#Form the paths of the 3 results files
filename = "Results.txt"
baseDir = "/ServerBenchmark"
home = os.getenv("HOME")
seqPath = home + baseDir + "/Sequential_Server/" + filename
heavyPath = home + baseDir+ "/Heavy_Server/" + filename
preHeavyPath = home + baseDir + "/Pre_Heavy_Server/" + filename

files = []
servers = []

#Checks which results files have been created
if(os.path.isfile(seqPath)):
    files.append(open(seqPath))
    servers.append("Sequential")
if(os.path.isfile(heavyPath)):
    files.append(open(heavyPath))
    servers.append("Heavy")
if(os.path.isfile(preHeavyPath)):
    files.append(open(preHeavyPath))
    servers.append("Pre Heavy")

fig = plt.figure()

#Create the 3 subplots
createPlot(files, servers, fig, 221, 1, 2, 
    "Total time (s)", "Requests", "Total time vs Number of requests")
createPlot(files, servers, fig, 222, 1, 3, 
    "Average time (s)", "Requests", "Average time vs Number of requests")
createPlot(files, servers, fig, 212, 0, 4, 
    "Average response time (s)", "Client threads", "Response time vs Number client of threads")

#Close all Results.txt files
for f in files:
    f.close()

plt.show()
