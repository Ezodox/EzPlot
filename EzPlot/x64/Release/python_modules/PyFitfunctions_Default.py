import numpy
import scipy
def GaussCDF(x, A, SD, EV): return A * 1/2 * (1 + scipy.special.erf(1/numpy.sqrt(2.*SD**2) * (x-EV)))
def SkewedGaussPDF(x, A, gSD, gEV, shape): return A * GaussPDF(x, 2, gSD, gEV) * GaussCDF(shape*x, 1, gSD, shape*gEV)
def GaussPDF(x, A, SD, EV): return A / numpy.sqrt(2.*numpy.pi*SD**2) * numpy.exp(-(x-EV)**2/(2.*SD**2))
def Linear(x, A, B): return A*x+B
def Quadratic(x, A, B, C): return A*x**2+B*x+C
