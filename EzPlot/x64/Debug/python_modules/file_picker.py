import numpy
import pandas

def GetColNames(FilePath):
    Data = pandas.read_excel(FilePath)
    return list(Data.columns)