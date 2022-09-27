import pandas

def GetColNames(FilePath, Seperator, Decimal):
    if FilePath.endswith("xlsx"):
        Data = pandas.read_excel(FilePath)
    elif FilePath.endswith("csv"):
        Data = pandas.read_csv(FilePath, sep=Seperator, decimal=Decimal)
    Names = list(Data.columns)
    return Names