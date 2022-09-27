#!/usr/bin/env python
# coding: utf-8

# In[1]:


# Python plotting and fitting script by Ezodox
# v1.0.0
# Licence: MIT License
# required packages: matplotlib, scipy, scikit-learn, pandas, numpy, copy

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import scipy as sp
from scipy import odr
from sklearn.metrics import r2_score
import math
from matplotlib.patches import ConnectionPatch
import copy
import os

# In[2]:

#--------------------------------Define Functions--------------------------------

def GaussCDF(x, A, SD, EV): 
    return A * 1/2 * (1 + sp.special.erf(1/np.sqrt(2.*SD**2) * (x-EV)))

def GaussPDF(x, A, SD, EV): 
    return A / np.sqrt(2.*np.pi*SD**2) * np.exp(-(x-EV)**2/(2.*SD**2))

def SkewedGaussPDF(x, A, gSD, gEV, shape): 
    return A * GaussPDF(x, 2, gSD, gEV) * GaussCDF(shape*x, 1, gSD, shape*gEV)

def MinusGaussCDF(x, A, SD, EV):
    return A * 1/2 * (1 - sp.special.erf((x-EV)/np.sqrt(2.*SD**2)))

def Voigt(x, A, SD, EV, Gamma):
    return A*sp.special.voigt_profile(x-EV,SD,Gamma)

def Quadratic(x, a, b, c):
    return a*x**2+b*x+c

def VoigtUnder(x, A, SD, EV, Gamma, a, b, c):
    return Voigt(x, A, SD, EV, Gamma)+Quadratic(x, a, b, c)

def VoigtUnderLinear(x, A, SD, EV, Gamma, a, b):
    return Voigt(x, A, SD, EV, Gamma)+Linear(x, a, b)

def VoigtUnderFix(x, A, SD, EV, a, b, c):
    return Voigt(x, A, SD, EV, SD)+Quadratic(x, a, b, c)

def VoigtUnderFixLinear(x, A, SD, EV, a, b):
    return Voigt(x, A, SD, EV, SD)+Linear(x, a, b)

def DoubleVoigtUnder(x, A1, SD1, EV1, Gamma1, A2, SD2, EV2, Gamma2, a, b, c):
    return Voigt(x, A1, SD1, EV1, Gamma1)+Voigt(x, A2, SD2, EV2, Gamma2)+Quadratic(x, a, b, c)

def DoubleVoigtUnderLinear(x, A1, SD1, EV1, Gamma1, A2, SD2, EV2, Gamma2, a, b):
    return Voigt(x, A1, SD1, EV1, Gamma1)+Voigt(x, A2, SD2, EV2, Gamma2)+Linear(x, a, b)

def DoubleVoigtUnderFix(x, A1, SD1, EV1, A2, SD2, EV2, a, b, c):
    return Voigt(x, A1, SD1, EV1, SD1)+Voigt(x, A2, SD2, EV2, SD2)+Quadratic(x, a, b, c)

def DoubleVoigtUnderFixLinear(x, A1, SD1, EV1, A2, SD2, EV2, a, b):
    return Voigt(x, A1, SD1, EV1, SD1)+Voigt(x, A2, SD2, EV2, SD2)+Linear(x, a, b)

def KleinNishina(x):
    Energy = 622
    EnergyDiv = 1/(1+Energy/511*(1-np.cos(np.radians(x))))
    DifW =1/2*(2.8179403227E-13)**2*EnergyDiv**2*(EnergyDiv+1/EnergyDiv-np.sin(np.radians(x))**2)
    return DifW
    
def Thomson(x):
    DifW =1/2*(2.8179403227E-13)**2*(1+np.cos(np.radians(x)))
    return DifW

def RutherfordFit(x, a, x_0):
    return  a / np.sin(np.radians((x+x_0)/2))**4

def Linear(x, A, B):
    return A*x+B

def ExpFit(x,A,k):
    return A * np.exp(k*x)

def FallingExpFit(x, A, k):
    return A * np.exp(-k*x)

def LimLossFit(func, sParams, xDat, yDat, sigma, lossfun, bounds, scale=1):
    def ResFun(params, x, y):
        return (func(x, *tuple(params)) - y) / sigma
    res_robust = sp.optimize.least_squares(ResFun, sParams, args=(xDat,yDat), loss=lossfun, bounds=bounds, f_scale=scale)
    return res_robust

def LSpErr(res):
    U, s, Vh = sp.linalg.svd(res.jac, full_matrices=False)
    tol = np.finfo(float).eps*s[0]*max(res.jac.shape)
    w = s > tol
    cov = (Vh[w].T/s[w]**2) @ Vh[w]  # robust covariance matrix
    perr = np.sqrt(np.diag(cov))     # 1sigma uncertainty on fitted parameters
    return perr

# Paramaters of ApplyFit:
# func: Fitfunction
# sParams: starting parameters
# DataNo: which data to use (number)
# Area: Area where to fit (min,max)
# ExArea: Excluded Area
# pArea: Area where to plot the fit: (min,max) or "fit" for fit area 
#        or "cross" for x-intersection (y=0) to x-intersection or (min,"cross") or ("cross",max)
#        or (min, ("cross", value)) to plot to a until a specific y-value is reached
# ExEr: Exclude the errors of the excluded area?
# pRes: Print all residuals (single errors)?
# LogFit: Use logarithmus on data and fit function before fitting?
# LogBase: base of log when using LogFit
# Loss: Sets a Loss function to limits a maximum loss on a single residual
# LossScale: Scale for loss
# odrType: 0 = explicit odr, 1 = implicit odr, 2 = ordinary least squares (OLS) for linear
# CV: Calculate Goodness of Fit with cross validation?
# FitOrders can be a list over multiple data sets
# FitOrdersZoom can be a list over zoom sets 

def ApplyFit(xDatas, yDatas, xErrors, yErrors, func, sParams, LatexFuncs=None, LatexParams=None, DataNo = 0, Area = None, 
             Color = "blue", Name=None, ExArea = (0,0), pArea=None, Line="-", ExEr=True, 
             pRes=False, Bounds=(-np.inf,np.inf), Method="lm", LogFit = False, LogBase = np.exp, 
             Loss = False, LossScale = 1, odrType = 0, CV = False, FitLinewidth = 3, FitOrder = 3, 
             FitOrdersZoom = 3):

    if Line == "dashdotdot": Line = (0, (3, 5, 1, 5, 1, 5))
    elif Line == "densely dashed": Line = (0, (5, 1))
    if LogFit == False: LogBase = False
    
    # Select Fit area
    xData, yData, xError, yError = xDatas, yDatas, xErrors, yErrors
    if type(xDatas) == list: xData = xDatas[DataNo]
    if type(yDatas) == list: yData = yDatas[DataNo]
    if type(xErrors) == list: xError = xErrors[DataNo]
    if type(yErrors) == list: yError = yErrors[DataNo]
    if type(xError) == list: xError = np.resize(xError,len(yError))
    xData = np.array(xData)
    yData = np.array(yData)

    # Remove every point without a y value
    xData = xData[~np.isnan(yData)]
    if isinstance(xError, pd.Series): xError = xError[~np.isnan(yData)]
    if isinstance(yError, pd.Series): yError = yError[~np.isnan(yData)]
    yData = yData[~np.isnan(yData)]
    
    if Area == None: Area = (min(xData),max(xData))
    x_fit = xData[(xData >= Area[0]) & (xData <= Area[1]) & ((xData<=ExArea[0]) | (xData>=ExArea[1]))]
    y_fit = yData[(xData >= Area[0]) & (xData <= Area[1]) & ((xData<=ExArea[0]) | (xData>=ExArea[1]))]
    xErr_fit = xError
    yErr_fit = yError
    if isinstance(xErr_fit, (int,float,np.floating)):
        if xErr_fit == 0: xErr_fit = None
        else: xErr_fit = np.full(len(x_fit),xError)
    else:
        xError = np.array(xError)
        xErr_fit = xError[(xData >= Area[0]) & (xData <= Area[1]) & ((xData<=ExArea[0]) | (xData>=ExArea[1]))]
    if isinstance(yErr_fit, (int,float,np.floating)):
        if yErr_fit == 0: yErr_fit = None
        else: yErr_fit = np.full(len(y_fit),yError)
    else:
        yError = np.array(yError)
        yErr_fit = yError[(xData >= Area[0]) & (xData <= Area[1]) & ((xData<=ExArea[0]) | (xData>=ExArea[1]))]
        
    # Add small Error to all Errors to prevent division by zero
    if isinstance(xErr_fit, np.ndarray): xErr_fit = xErr_fit + 1e-10
    if isinstance(yErr_fit, np.ndarray): yErr_fit = yErr_fit + 1e-10
    
    # Calculate fit parameters
    p,perr,pcov = CalcFit(func, sParams, x_fit, y_fit, xErr_fit, yErr_fit, method=Method, 
                            LogBase=LogBase, bounds=Bounds, loss=Loss, scale=LossScale, odrType=odrType)
    
    # Print Name of Fit
    if Name: print(Name+":")
    
    # Get parameter names
    pNames = func.__code__.co_varnames
    
    # Print found fit parameters
    print("      Parameters:")
    for i in range(len(p)):
        print("""
            {0} = {1:.10g} +- {2:.10g}
        """.format(pNames[i+1],p[i],perr[i]))

    #if not ExEr: 
    #    x_fit = xData
    #    y_fit = yData
    #    yErr_fit = yError
    #    xErr_fit = xError
        
    FitParams, MeanLine = CalcFitEr(x_fit, y_fit, xErr_fit, yErr_fit, func, params=p, 
                                    LatexFuncs=LatexFuncs, LatexParams=LatexParams, 
                                    pErr=perr, pRes=pRes, CV=CV, method=Method, 
                                    LogBase=LogBase, bounds=Bounds, loss=Loss, 
                                    scale=LossScale, Name = "Fit 1")
    
    ax = plt.gca()
    if type(pArea) == str:
        if pArea == "fit": pArea = Area
        elif pArea == "cross": pArea = ("cross","cross")
        elif pArea == "": pArea = None
    if type(pArea) != tuple and pArea == None:   
        # When no set xLimit, plot to visible area
        pArea = ax.get_xlim()
    
    # if pCross calculate x-intersection area
    for i in range(len(pArea)):
        if pArea[i] == None or (type(pArea[i]) == str and pArea[i] == ""):
            pAreaList = list(pArea)
            pAreaList[i] = ax.get_xlim()[i]
            pArea = tuple(pAreaList)
        if type(pArea[i]) == str and pArea[i] == "fit":
            pAreaList = list(pArea)
            pAreaList[i] = Area[i]
            pArea = tuple(pAreaList)
        if (type(pArea[i]) == str and pArea[i] == "cross") or (type(pArea[i]) == tuple and pArea[i][0] == "cross"):
            xPoints = np.linspace(*tuple(ax.get_xlim()),10000)
            StartX = xPoints[0]
            if i != 0: StartX = pArea[0]
            StartY = func(StartX,*tuple(p))
            CrossValue = 0
            if type(pArea[i]) == tuple: 
                CrossValue = pArea[i][1]
            Above = False
            if StartY >= CrossValue: Above = True
            CrossX = False
            for x in xPoints:
                y = func(x, *tuple(p))
                if (Above and y < CrossValue) or (not Above and y >= CrossValue): 
                    CrossX = x
                    break
            if not CrossX: CrossX = xPoints[-1]
            pAreaList = list(pArea)
            pAreaList[i] = CrossX
            pArea = tuple(pAreaList)
    
    # Calculate Fitfunction
    x_p = np.linspace(pArea[0], pArea[1], 2000)
    x_p1 = x_p[x_p <= ExArea[0]]
    x_p2 = x_p[x_p >= ExArea[1]]
    y_p1 = func(x_p1, *tuple(p))
    y_p2 = func(x_p2, *tuple(p))

    # Plot Fitfunction
    FitLine, = plt.plot(x_p1, y_p1, marker='None', linestyle=Line, color=Color, zorder=FitOrder, linewidth=FitLinewidth)
    plt.plot(x_p2, y_p2, marker='None', linestyle=Line, color=Color, zorder=FitOrder, linewidth=FitLinewidth)

    fig = plt.gcf()
    Axes = fig.get_axes()
    if len(Axes) > 1:    
        FitOrderZoom = FitOrdersZoom
        for sub in Axes:
            if type(FitOrdersZoom) == list: FitOrderZoom = FitOrdersZoom[Axes.index(sub)]
            sub.plot(x_p1, y_p1, marker='None', linestyle=Line, color=Color, zorder=FitOrderZoom, linewidth=FitLinewidth)
            sub.plot(x_p2, y_p2, marker='None', linestyle=Line, color=Color, zorder=FitOrderZoom, linewidth=FitLinewidth)
    
    # if fitted an underground plot it
    UnderLine = False
    if "Under" in func.__name__:
        pNames = func.__code__.co_varnames
        Uparams = []
        for n in pNames:
            if n == "a" or n == "b" or n == "c":
                Uparams.append(p[pNames.index(n)-1])
        if len(Uparams) == 3:
            def UFunc(x, a, b, c):
                return Quadratic(x, a, b, c)
        elif len(Uparams) == 2:
            def UFunc(x, a, b):
                return Linear(x, a, b)
        y_p1 = UFunc(x_p1, *tuple(Uparams))
        y_p2 = UFunc(x_p2, *tuple(Uparams))
        UnderLine, = plt.plot(x_p1, y_p1, marker='None', linestyle="--", color="red", zorder=FitOrder, linewidth=FitLinewidth)
        plt.plot(x_p2, y_p2, marker='None', linestyle="--", color="red", zorder=FitOrder, linewidth=FitLinewidth)
        if len(Axes) > 1:
            for sub in Axes:
                sub.plot(x_p1, y_p1, marker='None', linestyle="--", color="red", zorder=FitOrder, linewidth=FitLinewidth)
                sub.plot(x_p2, y_p2, marker='None', linestyle="--", color="red", zorder=FitOrder, linewidth=FitLinewidth)
        
    return FitLine, UnderLine, MeanLine, FitParams

def CalcFit(func, params, xdat, ydat, xerr, yerr, method="lm", LogBase=False, bounds=(-np.inf,np.inf), 
            loss=False, scale=1, odrType=0):
    
    # Define Fit function for Log and odr (switch argument order for odr)
    if method == "odr":
        def FitFunc(args, x):
            if LogBase: return np.log(func(x, *args)) / np.log(LogBase)
            else: return func(x, *args)
    else:
        def FitFunc(x, *args):
            if LogBase: return np.log(func(x, *args)) / np.log(LogBase)
            else: return func(x, *args)
    if LogBase:
        xdat = xdat[ydat != 0]
        yerr = yerr[ydat != 0]
        ydat = ydat[ydat != 0]
        yerr = yerr / ydat # Propagation of uncertainty: error of log(y) is yerr / y
        ydat = np.log(ydat) / np.log(LogBase)
    if loss:
        ResLim = LimLossFit(FitFunc, params, xdat, ydat, sigma=yerr, lossfun=loss, bounds=bounds, scale=scale)
        p = ResLim.x
        perr = LSpErr(ResLim)
        pcov = None
    elif method=="odr":
        odrData = odr.RealData(xdat, ydat, xerr, yerr)
        model = odr.Model(FitFunc)
        myodr = odr.ODR(odrData, model, params)
        myodr.set_job(fit_type=odrType)
        output = myodr.run()
        p = output.beta
        perr = output.sd_beta
        pcov = output.cov_beta
        #chi2 = output.res_var
        #print("odr chi2:",chi2)
    else:
        p, pcov = sp.optimize.curve_fit(FitFunc, xdat, ydat, sigma=yerr,
                                p0=params, method=method,bounds=bounds)
        perr = np.sqrt(np.diag(pcov))
    return p, perr, pcov

def CalcFitEr(xdat, ydat, xerr, yerr, func, params, LatexFuncs=None, LatexParams=None, pErr=0, 
              pRes=True, CV=False, method="lm", LogBase=False, bounds=(-np.inf,np.inf), loss=False, 
              scale=1, Name="Fit 1"):
    
    # Calculate confidence interval with 95%
    #DOF = len(yData)-len(sParams) # Degrees of Freedom = number of data points - number of (non fixed) parameters
    #tInterval = scipy.stats.t.interval(0.95,DOF)
    #cIntervall = tInterval[1]*perr
    
    if CV: # Cross Validation
        CVres = []
        for n in range(len(xdat)): # delete n-th data point, fit data and calculate n-residual
            CVxdat = np.delete(xdat,n)
            CVydat = np.delete(ydat,n)
            CVxerr = np.delete(xerr,n)
            CVyerr = np.delete(yerr,n)
            CVp,CVperr,CVpcov = CalcFit(func, params, CVxdat, CVydat, CVxerr, CVyerr, method=method, 
                                        LogBase=LogBase, bounds=bounds, loss=loss, scale=scale)
            CVModelYn = func(xdat[n], *tuple(CVp))
            CVresn = np.abs(CVModelYn - ydat[n])
            CVres = np.append(CVres,CVresn)
            #CVLikeli = NormalDichte(ydat[n],1,yerr,CVModelYn) # Likelihood
        CVSE = np.square(CVres) # squared errors / residuals
        CVMSE = np.mean(CVSE) # mean squared errors
        CVRMSE = np.sqrt(CVMSE) # Root Mean Squared Error, RMSE
        print("      Cross Validation RMSE:", CVRMSE)
    
    # Calculate RMSE,R-squared and print
    ModelY = func(xdat, *tuple(params))
    Residuals = np.abs(ModelY - ydat)
    SE = np.square(Residuals) # squared errors / residuals
    MSE = np.mean(SE) # mean squared errors
    RMSE = np.sqrt(MSE) # Root Mean Squared Error, RMSE
    
    if LogBase:
        yerr = yerr[ModelY != 0]
        ModelY = ModelY[ModelY != 0]
        ydat = ydat[ydat != 0]
        yerr = yerr / ydat # Propagation of uncertainty: error of log(y) is yerr / y
        ModelY = np.log(ModelY) / np.log(LogBase)
        ydat = np.log(ydat) / np.log(LogBase)
        Residuals = np.abs(ModelY - ydat)
    
    R2 = r2_score(ydat, ModelY) # R-squared
    AdjR2 = 1 - ((1-R2)*(len(xdat)-1)/(len(xdat)-len(params)-1))
    YErrNotZero =  not (type(yerr) == float and yerr == 0)
    if YErrNotZero:
        nRes = Residuals/yerr # normalized Residuals
        Chi2 = np.sum(nRes**2) # Chi-squared
        DoF = len(xdat) - len(params) # Degrees of Freedom = amount of data - amount of parameters
        rChi2 = Chi2 / DoF
    
    print("      RMSE:", RMSE)
    print("      R-squared:", R2)
    print("      Adjusted R-squared:", AdjR2)
    if YErrNotZero:
        print("      Chi-squared:",Chi2)
        print("      Reduced Chi-squared:",rChi2)
    if pRes:
        if YErrNotZero:
            #GoodRes = nRes[nRes <= 3]
            #NumGoodRes = len(GoodRes)
            #MgRes = np.mean(GoodRes)
            print("      ----------Normalized Residuals----------")
            for r in nRes: print("      "+str(r))
            print("      ----------------------------------------")
        else:
            print("      ----------Residuals----------")
            for r in Residuals: print("      "+str(r))
            print("      -----------------------------")
    print("")
    FitParams = None
    if LatexFuncs and LatexParams: 
        FitParams = {}
        FitParams[Name] = {}
        FitParams[Name]["Func"] = copy.deepcopy(LatexFuncs[func.__name__])
        FitParams[Name]["Params"] = copy.deepcopy(LatexParams[func.__name__])
        FitParams[Name]["ParamVals"] = params
        FitParams[Name]["ParamErrs"] = pErr
        if YErrNotZero: 
            FitParams[Name]["Params"].append("\\chi^2 / \\mathrm{DoF}")
            FitParams[Name]["ParamVals"] = np.append(FitParams[Name]["ParamVals"],rChi2)
            FitParams[Name]["ParamErrs"] = np.append(FitParams[Name]["ParamErrs"],0)
        if func.__name__ == "Linear" or "Exp" in func.__name__:
            FitParams[Name]["Params"].append("R^2")
            FitParams[Name]["ParamVals"] = np.append(FitParams[Name]["ParamVals"],AdjR2)
            FitParams[Name]["ParamErrs"] = np.append(FitParams[Name]["ParamErrs"],0)
    MeanLine = False
    if "Voigt" in func.__name__:
        NumVoigts = 1
        UnderLen = 0
        if "Double" in func.__name__: NumVoigts = 2
        if "Under" in func.__name__: UnderLen = 3
        if "Linear" in func.__name__: UnderLen = 2
        for i in range(NumVoigts):
            NumParams = int(round((len(params)-UnderLen) / NumVoigts))
            A = params[0+i*NumParams]
            SD = params[1+i*NumParams]
            Aerr = pErr[0+i*NumParams]
            SDerr = pErr[1+i*NumParams]
            if "Fix" in func.__name__:
                Gamma = SD
                GammaErr = SDerr
            else:
                Gamma = params[3+i*NumParams]
                GammaErr = pErr[3+i*NumParams]
            Height = Voigt(0, A, SD, 0, Gamma)
            #Height2 = Voigt(0, A-Aerr, SD+SDerr, 0, Gamma+GammaErr)
            #HeightErr = abs(Height-Height2)
            GaussFWHM = np.sqrt(8*np.log(2))*SD
            LorentzFWHM = 2*Gamma
            FWHM = 0.5346*LorentzFWHM + np.sqrt(0.2166*LorentzFWHM**2 + GaussFWHM**2)
            #GaussFWHM = np.sqrt(8*np.log(2))*(SD+SDerr)
            #LorentzFWHM = 2*(Gamma+GammaErr)
            #VoigtFWHM2 = 0.5346*LorentzFWHM + np.sqrt(0.2166*LorentzFWHM**2 + GaussFWHM**2)
            #FWHMErr = abs(VoigtFWHM2 - VoigtFWHM)
            k1 = 2*0.5346
            k2 = 0.2166*4
            k3 = 8*np.log(2)
            FWHMErr = np.sqrt((k2 * Gamma / np.sqrt(k2*Gamma**2 + k3*SD**2) + k1)**2 * GammaErr**2 
                            + (k3 * SD / np.sqrt(k3*SD**2 + k2*Gamma**2))**2 * SDerr**2)
            HeightErrA = Voigt(0,1,SD,0,Gamma) * Aerr
            HeightErrSD = (-(A*sp.special.erfc(Gamma/(np.sqrt(2)*SD))*np.exp(Gamma**2/(2*SD**2)))/(np.sqrt(2)*SD**2)-
                            (A*Gamma**2*sp.special.erfc(Gamma/(np.sqrt(2)*SD))*np.exp(Gamma**2/(2*SD**2)))/(np.sqrt(2)*SD**4)+
                            (A*Gamma)/(np.sqrt(np.pi)*SD**3)) * SDerr
            HeightErrGamma = ((A*Gamma*np.exp(Gamma**2/(2*SD**2))*sp.special.erfc(Gamma/(np.sqrt(2)*SD)))
                                /(np.sqrt(2)*SD**3)-A/(np.sqrt(np.pi)*SD**2)) * GammaErr
            HeightErr = np.sqrt(HeightErrA**2 + HeightErrSD**2 + HeightErrGamma**2)
            if LatexFuncs and LatexParams:
                ApStr = ""
                if NumVoigts == 2: ApStr = "\\text{ "+str(i+1)+"}"
                FitParams[Name]["Params"].append("\\text{Height}"+ApStr)
                FitParams[Name]["ParamVals"] = np.append(FitParams[Name]["ParamVals"], Height)
                FitParams[Name]["ParamErrs"] = np.append(FitParams[Name]["ParamErrs"], HeightErr)
                FitParams[Name]["Params"].append("\\text{FWHM}"+ApStr)
                FitParams[Name]["ParamVals"] = np.append(FitParams[Name]["ParamVals"], FWHM)
                FitParams[Name]["ParamErrs"] = np.append(FitParams[Name]["ParamErrs"], FWHMErr)
            
            NumVoigtStr = ""
            if NumVoigts == 2: NumVoigtStr = str(i+1)
            print("      Height {0}: {1:.10g} +- {2:.10g}".format(NumVoigtStr, Height, HeightErr))
            print("      FWHM {0}: {1:.10g} +- {2:.10g}".format(NumVoigtStr, FWHM, FWHMErr))
    if "Skewed" in func.__name__:
        NumSkewed = 1
        UnderLen = 0
        if "Double" in func.__name__: NumSkewed = 2
        if "Under" in func.__name__: UnderLen = 3
        if "Linear" in func.__name__: UnderLen = 2
        for i in range(NumSkewed):
            NumParams = int(round((len(params)-UnderLen) / NumSkewed))
            A = params[0+i*NumParams]
            gSD = np.abs(params[1+i*NumParams])
            gEV = params[2+i*NumParams]
            shape = params[3+i*NumParams]
            Mean = gEV + gSD * (shape / np.sqrt(1+ shape**2)) * np.sqrt(2 / np.pi)
            MeanY = (A / (gSD * np.sqrt(2* np.pi)) * np.exp(-shape**2/(np.pi*(1+shape**2))) 
                * (1 + sp.special.erf(shape**2/np.sqrt(np.pi*(1+shape**2)))))
            Aerr = pErr[0+i*NumParams]
            gSDerr = pErr[1+i*NumParams]
            gEVerr = pErr[2+i*NumParams]
            shapeerr = pErr[3+i*NumParams]
            MeanErr = np.sqrt(gEVerr**2 + ((shape / np.sqrt(1+ shape**2)) * np.sqrt(2 / np.pi) * gSDerr)**2
                                + (((gSD / (np.sqrt(1+ shape**2))**3) * np.sqrt(2 / np.pi))* shapeerr)**2)
            MeanYErr = np.sqrt((Aerr * (1 / (gSD * np.sqrt(2* np.pi)) * np.exp(-shape**2/(np.pi*(1+shape**2))) 
                * (1 + sp.special.erf(shape**2/np.sqrt(np.pi*(1+shape**2))))))**2 
                + (gSDerr * A / (gSD**2 * np.sqrt(2 * np.pi)) * np.exp(-shape**2/(np.pi*(1+shape**2))) 
                * (1 + sp.special.erf(shape**2/np.sqrt(np.pi*(1+shape**2)))))**2 + (((A*np.sqrt(2)*shape*np.exp(-shape**2/np.pi)
                *((shape**2+1)**(3/2)*np.exp(shape**4/(np.pi*(shape**2+1)))*(sp.special.erf(shape**2/(np.sqrt(np.pi)
                *np.sqrt(shape**2+1)))+1)-(shape**2+1)**2*(shape**2+2)))/(np.pi**(3/2)*gSD*(shape**2+1)**(7/2))) * shapeerr)**2)
            if LatexFuncs and LatexParams:
                ApStr = ""
                if NumSkewed == 2: ApStr = "\\text{ "+str(i+1)+"}"
                FitParams[Name]["Params"].append("Mean"+ApStr)
                FitParams[Name]["ParamVals"] = np.append(FitParams[Name]["ParamVals"], Mean)
                FitParams[Name]["ParamErrs"] = np.append(FitParams[Name]["ParamErrs"], MeanErr)
                FitParams[Name]["Params"].append("f(Mean)"+ApStr)
                FitParams[Name]["ParamVals"] = np.append(FitParams[Name]["ParamVals"], MeanY)
                FitParams[Name]["ParamErrs"] = np.append(FitParams[Name]["ParamErrs"], MeanYErr)
            
            NumSkewedStr = ""
            if NumSkewed == 2: NumSkewedStr = str(i+1)
            print("      Mean {0}: {1:.10g} +- {2:.10g}".format(NumSkewedStr, Mean, MeanErr))
            print("      MeanY {0}: {1:.10g} +- {2:.10g}".format(NumSkewedStr, MeanY, MeanYErr))
            
            fig = plt.gcf()
            Axes = fig.get_axes()
            for sub in Axes:
                if not MeanLine:
                    MeanLine = sub.axvline(x = Mean, color = "green")
                else:
                    sub.axvline(x = Mean, color = "green")
        
    return FitParams, MeanLine
         
def SaveParamsAsLatex(FitParams, FilePath = ""):
    Alignment, Names, Funcs, TitleLine, Titles = "","","","",""
    for i,n in enumerate(FitParams): 
        Alignment = Alignment + "l l "
        Names = Names + "\\multicolumn{2}{c}{"+n+"} & "
        Funcs = Funcs + "\\multicolumn{2}{c}{"+"$"+FitParams[n]["Func"]+"$"+"} & "
        TitleLine = TitleLine + "\\cmidrule(lr){"+str(2*i+1)+"-"+str(2*i+2)+"} "
        Titles = Titles + "Fitparameter & Wert & "
        
    Alignment = "@{} " + Alignment + "@{}"
    Names = Names[:-3]
    Funcs = Funcs[:-3]
    Titles = Titles[:-3]
    
    # Remove first l and last r in first/last cmidrule(lr)
    lIndex = TitleLine.find("l",10)
    TitleLine = TitleLine[:lIndex] + TitleLine[lIndex+1:]
    rIndex = TitleLine.rfind("r")
    TitleLine = TitleLine[:rIndex] + TitleLine[rIndex+1:]
    
    Header = [
        "\\begin{table}[H]", 
        "\\centering",
        "\\begin{tabular}{"+Alignment+"}",
        "\\toprule",
        Names + " \\\\",
        Funcs + " \\\\",
        TitleLine,
        Titles + " \\\\",
        "\\midrule"
    ]
    Footer = [
        "\\bottomrule",
        "\\end{tabular}",
        "\\end{table}"
    ]
    if FilePath != "": FilePath = FilePath + "\\"
    with open(FilePath+"plot.tex", "w") as f:
        for line in Header:
            f.write(line)
            f.write("\n")
        LinesStr = []
        for k,n in enumerate(FitParams): # iterate over k-Fits
            for i,p in enumerate(FitParams[n]["Params"]): # iterate over i-parameters of Fit k
                Par = FitParams[n]["ParamVals"][i]
                if not ("chi^2" in p) and not ("R^2" in p): Err = FitParams[n]["ParamErrs"][i]
                else: Err = 0
                # Significant Digits = 2 + (Power of ten of Param - Power of ten of Error)
                if Err == 0: 
                    if abs(Par) > 10: SigDig = 0
                    elif abs(Par) > 1: SigDig = 1
                    else: SigDig = 2
                    StrPar = "{0:.{1}f}".format(Par,SigDig)
                else:
                    PoTpar = math.floor(math.log10(abs(Par)))
                    PoTerr = math.floor(math.log10(Err))
                    SigDig = 2 + (PoTpar - PoTerr)
                    SigDigErr = 2
                    if SigDig < 2: # if power of ten of Error is bigger than for Parameter:
                        SigDig = 1 # Set siginificant digits of parameter to 1
                        SigDigErr = 1 + abs(PoTpar - PoTerr)
                    ShowZero = "#" # Show Zero after decimal point in g-format
                    if SigDig == 1: ShowZero = "" # dont Show Zero after decimal point in g-format
                    StrPar = "{0:{1}.{2}g}".format(Par,ShowZero,SigDig)
                    if StrPar[-1] == ".": StrPar = StrPar[:-1] #if last character ist a dot remove it
                    StrErr = "{0:#.{1}g}".format(Err,SigDigErr)
                    StrErr = StrErr.replace(".","") # remove decialpoint
                    StrErr = StrErr.split("e")[0] # remove exponential
                    StrErr = StrErr.lstrip("0") # remove leading zeroes
                    if "e" in StrPar.lower(): StrPar = StrPar.replace("e","("+StrErr+")e")
                    else: StrPar = StrPar + "("+StrErr+")"
                #StrErr = "{0:.2g}".format(Err)
                #if "e" in StrPar.lower(): # if found remove "e" from Parmeter and bring Error to form of Parameter
                #    StrPar = StrPar.split("e")[0]
                #    StrErr = StrErr.split("e")[0]
                #    Err = float(StrErr) / 10**(SigDig-2)
                #    Err = round(Err,SigDig)
                #    StrErr =str(Err)
                #    StrErr = StrErr + "e" + str(math.floor(math.log10(abs(Par))))
                #CurrentCell = "$"+p+"$"+" & "+ "$\\num{" +StrPar + " \\pm " + StrErr + "}$ & "
                #if Err == 0: CurrentCell = "$"+p+"$"+" & "+ "$\\num{" +StrPar+"}$ & "
                CurrentCell = "$"+p+"$"+" & "+ "$\\num{" +StrPar+"}$ & "
                if k != 0: #if not first Fit, check if cell in last columns are empty
                    for FitNum in range(len(FitParams)):
                        if FitNum==k: break
                        FitName = list(FitParams)[FitNum]
                        if i > (len(FitParams[FitName]["Params"]) - 1): # if cell in last column was empty (more params)
                            CurrentCell = " & & " + CurrentCell # add empty cell of the column before
                if len(LinesStr) == i: # if line does not exist yet begin new line with cell
                    LinesStr.append(CurrentCell)
                else: LinesStr[i] = LinesStr[i] + CurrentCell # if line does already exist append cell to line
                if k == len(FitParams)-1: # if end of Fits reached write line in file
                    LinesStr[i] = LinesStr[i][:-2]
                    LinesStr[i] = LinesStr[i] + " \\\\"
                    f.write(LinesStr[i])
                    f.write("\n")
                    ParamCounts = len(FitParams[n]["Params"])
                    if i == ParamCounts-1 and len(LinesStr) > ParamCounts: # if last param of last fit write all opened lines
                        for oL in range(i+1,len(LinesStr)):
                            oLFitNum = round(LinesStr[oL].count(" &") / 2) - 1
                            for ei in range(k - oLFitNum): # fill empty columns to end of fits
                                LinesStr[oL] = LinesStr[oL] + " & &" 
                            LinesStr[oL] = LinesStr[oL][:-2]
                            LinesStr[oL] = LinesStr[oL] + " \\\\"
                            f.write(LinesStr[oL])
                            f.write("\n")
        for line in Footer:
            f.write(line)
            f.write("\n")

def PickData(DataInfos):
    
    Path = DataInfos["Path"]
    xColumns = DataInfos["xColumns"]
    yColumns = DataInfos["yColumns"]
    xErrorColumns = DataInfos["xErrorColumns"]
    yErrorColumns = DataInfos["yErrorColumns"]
    Seperator = DataInfos["Seperator"]
    Decimal = DataInfos["Decimal"]
    
    if Path.endswith("xlsx"):
        Data = pd.read_excel(Path)
    elif Path.endswith("csv"):
        Data = pd.read_csv(Path, sep=Seperator, decimal=Decimal)
    if type(xColumns) == list:
        xDatas = []
        for col in xColumns: xDatas.append(Data[col])
    else: xDatas = Data[xColumns]
        
    if type(yColumns) == list:
        yDatas = []
        for col in yColumns: yDatas.append(Data[col])
    else: yDatas = Data[yColumns]
        
    if type(xErrorColumns) == list:
        xErrors = []
        for col in xErrorColumns: 
            if type(col) == str:
                if col == "[NULL]": xErrors.append(0)
                else: xErrors.append(Data[col])
            else: xErrors.append(col)
    elif type(xErrorColumns) == str: xErrors = Data[xErrorColumns]
    else: xErrors = xErrorColumns
        
    if type(yErrorColumns) == list:
        yErrors = []
        for col in yErrorColumns: 
            if type(col) == str:
                if col == "[NULL]": yErrors.append(0)
                else: yErrors.append(Data[col])
            else: yErrors.append(col)
    elif type(yErrorColumns) == str: yErrors = Data[yErrorColumns]
    else: yErrors = yErrorColumns
        
    return xDatas, yDatas, xErrors, yErrors
            
#--------------------------------Plot Figure--------------------------------
def PlotFigure(DataInfos, Settings):
    
    # Shorten variable names:
    FigWidth = Settings["FigWidth"]
    FigHeight = Settings["FigHeight"]
    Zoom = Settings["Zoom"]
    ZoomHelpLines = Settings["ZoomHelpLines"]
    Projection = Settings["Projection"]
    mColors = Settings["mColors"]
    mSizes = Settings["mSizes"]
    mStyles = Settings["mStyles"]
    mOrders = Settings["mOrders"]
    mAlphas = Settings["mAlphas"]
    mConnects = Settings["mConnects"]
    ErrWidths = Settings["ErrWidths"]
    ErrCapsizes = Settings["ErrCapsizes"]
    xLimit = Settings["xLimit"]
    yLimit = Settings["yLimit"]
    LogScaleX = Settings["LogScaleX"]
    LogScaleY = Settings["LogScaleY"]
    LogScaleBaseX = Settings["LogScaleBaseX"]
    LogScaleBaseY = Settings["LogScaleBaseY"]
    SciStyleX = Settings["SciStyleX"]
    SciStyleY = Settings["SciStyleY"]
    SciStyleXZoom = Settings["SciStyleXZoom"]
    SciStyleYZoom = Settings["SciStyleYZoom"]
    LabelX = Settings["LabelX"]
    LabelY = Settings["LabelY"]
    LabelSize = Settings["LabelSize"]
    TickLabelSize = Settings["TickLabelSize"]
    TickDirection = Settings["TickDirection"]
    MajorTickLength = Settings["MajorTickLength"]
    MajorTickWidth = Settings["MajorTickWidth"]
    MajorTicksPeriodX = Settings["MajorTicksPeriodX"]
    MajorTicksPeriodY = Settings["MajorTicksPeriodY"]
    MajorTicksPeriodXZoom = Settings["MajorTicksPeriodXZoom"]
    MajorTicksPeriodYZoom = Settings["MajorTicksPeriodYZoom"]
    MinorTicksX = Settings["MinorTicksX"]
    MinorTicksY = Settings["MinorTicksY"]
    MinorTicksPeriodX = Settings["MinorTicksPeriodX"]
    MinorTicksPeriodY = Settings["MinorTicksPeriodY"]
    MinorTickLength = Settings["MinorTickLength"]
    MinorTickWidth = Settings["MinorTickWidth"]
    # Remove Ticks with element ID of array (0) for first, (-1) for last, (0,1) for first and second, ...
    RemoveMajorXTicks = Settings["RemoveMajorXTicks"]
    RemoveMajorYTicks = Settings["RemoveMajorYTicks"]
    RemoveMinorXTicks = Settings["RemoveMinorXTicks"]
    RemoveMinorYTicks = Settings["RemoveMinorYTicks"]

    # Default Settings:
    if not FigWidth: FigWidth = 9.5
    if not FigHeight: FigHeight = 4.0
    if not Projection: Projection = False
    if not Zoom: Zoom = 0
    if not ZoomHelpLines: ZoomHelpLines = True;

    xDatas, yDatas, xErrors, yErrors = PickData(DataInfos)
        
    SizeY = FigHeight
    if type(Zoom) == list and len(Zoom) > 0: SizeY = FigHeight * 2
    fig = plt.figure(figsize = (FigWidth, SizeY), dpi = 80)

    if type(Zoom) == list and len(Zoom) > 0:
        plt.subplots_adjust(bottom = 0., left = 0, top = 1., right = 1)
        Subs = []
        for i in range(1,len(Zoom)+2):
            cell = i
            if i == len(Zoom)+1: cell = (i,2*len(Zoom)+1) # if end of loop cell is the complete last row
            sub = fig.add_subplot(2, len(Zoom), cell) # two rows, len of zoom columns, cell
            if i != len(Zoom)+1: Subs.append(sub)

    NumYData = 1
    if type(yDatas) == list: NumYData = len(yDatas)
    ScatterIDs = []
    for i in range(NumYData):
        xData, yData, xError, yError = xDatas, yDatas, xErrors, yErrors
        mSize, mStyle, mColor, mOrder, mAlpha, mConnect = mSizes, mStyles, mColors, mOrders, mAlphas, mConnects
        ErrCapsize, ErrWidth = ErrCapsizes, ErrWidths
        if type(xDatas) == list: xData = xData[i]
        if type(yDatas) == list: yData = yDatas[i]
        if type(xErrors) == list: xError = xErrors[i]
        if type(yErrors) == list: yError = yErrors[i]
        if type(mSizes) == list: mSize = mSizes[i]
        if type(mStyle) == list: mStyle = mStyles[i]
        if type(mColors) == list: mColor = mColors[i]
        if type(mOrders) == list: mOrder = mOrder[i]
        if type(mAlphas) == list: mAlpha = mAlphas[i]
        if type(mConnects) == list: mConnect = mConnects[i]
        if type(ErrCapsizes) == list: ErrCapsize = ErrCapsizes[i]
        if type(ErrWidth) == list: ErrWidth = ErrWidth[i]

        # Default Settings:
        if not mColor: mColor = "black"
        if not mSize: mSize = 20
        if not mStyle: mStyle = "o"
        if not mOrder: mOrder = 2
        if not mAlpha: mAlpha = 1
        if not mConnect: mConnect = False
        if not ErrWidth: ErrWidth = 2
        if not ErrCapsize: ErrCapsize = 3
        if not xLimit: xLimit = None
        if not yLimit: yLimit = None
        if not LogScaleX: LogScaleX = False
        if not LogScaleY: LogScaleY = False
        if not LogScaleBaseX: LogScaleBaseX = np.exp
        if not LogScaleBaseY: LogScaleBaseY = np.exp
        if not SciStyleX: SciStyleX = None
        if not SciStyleY: SciStyleY = None
        if not SciStyleXZoom: SciStyleXZoom = None
        if not SciStyleYZoom: SciStyleYZoom = None
        if not LabelX: LabelX = r"Degrees / $^\circ$"
        if not LabelY: LabelY = r"$\beta$ / km"
        if not LabelSize: LabelSize = 30
        if not TickLabelSize: TickLabelSize = 20
        if not TickDirection: TickDirection = "out"
        if not MajorTickLength: MajorTickLength = 10
        if not MajorTickWidth: MajorTickWidth = 2.5
        if not MajorTicksPeriodX: MajorTicksPeriodX = None
        if not MajorTicksPeriodY: MajorTicksPeriodY = None
        if not MajorTicksPeriodXZoom: MajorTicksPeriodXZoom = None
        if not MajorTicksPeriodYZoom: MajorTicksPeriodYZoom = None
        if not MinorTicksX: MinorTicksX = False
        if not MinorTicksY: MinorTicksY = False
        if not MinorTickLength: MinorTickLength = 5
        if not MinorTickWidth: MinorTickWidth = 1.5

        # Rescale xErrors to yErrors
        if type(xError) == list: xError = np.resize(xError,len(yError))

        # Remove every point without a y value
        xData = xData[~np.isnan(yData)]
        if isinstance(xError, pd.Series): xError = xError[~np.isnan(yData)]
        if isinstance(yError, pd.Series): yError = yError[~np.isnan(yData)]
        yData = yData[~np.isnan(yData)]

        if Projection == "polar":
            xData = xData * np.pi/180
            xError = xError * np.pi/180

        ScatterID = plt.scatter(x=xData, y = yData, s=mSize, marker=mStyle, color=mColor, alpha=mAlpha, zorder=mOrder)
        ScatterIDs = np.append(ScatterIDs, ScatterID)
        plt.errorbar(xData, yData, yerr=yError, xerr=xError, fmt="none", capsize=ErrCapsize, elinewidth=ErrWidth, 
                markersize=0, color=mColor,alpha = mAlpha)
        if mConnect: plt.plot(xData, yData,  marker='None', color=mColor, alpha=mAlpha, zorder=mOrder)

        if type(Zoom) == list and len(Zoom) > 0:
            for s in Subs:
                s.scatter(x=xData, y = yData, s=mSize, marker=mStyle, color=mColor, alpha=mAlpha, zorder=mOrder)
                s.errorbar(xData, yData, yerr=yError, xerr=xError, fmt="none", capsize=ErrCapsize, elinewidth=ErrWidth, 
                    markersize=0, color=mColor,alpha = mAlpha)
                if mConnect: s.plot(xData, yData,  marker='None', color=mColor, alpha=mAlpha, zorder=mOrder)

    #---------------------Apply settings on plot design-------------------------

    if LabelX: plt.xlabel(LabelX, fontsize = LabelSize)
    if LabelY: plt.ylabel(LabelY, fontsize = LabelSize)
    if LogScaleX: plt.xscale("log", base = LogScaleBaseX)
    if LogScaleY: plt.yscale("log", base = LogScaleBaseY)
    if Projection == "polar": 
        LabelX = LabelY
        LabelY = False
        ax = fig.add_subplot(projection = "polar")
        PxLimit = (xLimit[0]*np.pi/180,xLimit[1]*np.pi/180)
        plt.xlim(PxLimit)
        ax.xaxis.set_tick_params(pad=15)
        ax.get_yaxis().get_offset_text().set_visible(False)
        ax.set_thetamin(xLimit[0])
        ax.set_thetamax(xLimit[1])
    else: plt.xlim(xLimit)
    plt.ylim(yLimit)
    plt.minorticks_on()
    plt.tick_params(axis="both",which="both",direction=TickDirection, top=True, right=True, labelsize = TickLabelSize)
    plt.tick_params(axis="both",which="major",length=MajorTickLength, width=MajorTickWidth)
    plt.tick_params(axis="both",which="minor",length=MinorTickLength, width=MinorTickWidth,
                    bottom=MinorTicksX, top=MinorTicksX, left=MinorTicksY, right=MinorTicksY)

    plt.grid(alpha = .2)


    # Set scientific notation
    if SciStyleX: plt.ticklabel_format(axis="x",style="sci", scilimits=(SciStyleX,SciStyleX))
    if SciStyleY: plt.ticklabel_format(axis="y",style="sci", scilimits=(SciStyleY,SciStyleY), useMathText=True)

    # Get axis
    ax = plt.gca()

    # Set scientific notation label size to tick label size
    ax.xaxis.offsetText.set_fontsize(TickLabelSize)
    ax.yaxis.offsetText.set_fontsize(TickLabelSize)

    # Move scientific notation
    #ax.get_yaxis().get_offset_text().set_x(0.1)

    # Get limit of axis
    if not Projection == "polar": xLimit = ax.get_xlim()
    yLimit = ax.get_ylim()

    # Scientific Notation to normal notation for small numbers
    #ax.yaxis.set_major_formatter(ticker.FuncFormatter(lambda y, _: '{:g}'.format(y)))

    # Tick placing
    if MajorTicksPeriodX: ax.xaxis.set_major_locator(plt.MultipleLocator(MajorTicksPeriodX))
    if MajorTicksPeriodY: ax.yaxis.set_major_locator(plt.MultipleLocator(MajorTicksPeriodY))
    if MinorTicksPeriodX: ax.xaxis.set_minor_locator(plt.MultipleLocator(MinorTicksPeriodX))
    if MinorTicksPeriodY: ax.yaxis.set_minor_locator(plt.MultipleLocator(MinorTicksPeriodY))

    # Draw canvas to trigger tick positioning
    fig.canvas.draw()

    # Remove specific ticks (found the function set_visible for that in the github source)
    for i in RemoveMajorXTicks:
        xTicks = ax.xaxis.get_major_ticks()
        xTicks[i].tick1line.set_visible(False)
        xTicks[i].tick2line.set_visible(False)
    for i in RemoveMajorYTicks:
        yTicks = ax.yaxis.get_major_ticks()
        yTicks[i].tick1line.set_visible(False)
        yTicks[i].tick2line.set_visible(False)
    for i in RemoveMinorXTicks:
        xMinTicks = ax.xaxis.get_minor_ticks()
        xMinTicks[i].tick1line.set_visible(False)
        xMinTicks[i].tick2line.set_visible(False)
    for i in RemoveMinorYTicks:
        yMinTicks = ax.yaxis.get_minor_ticks()
        yMinTicks[i].tick1line.set_visible(False)
        yMinTicks[i].tick2line.set_visible(False)

    # Remove x-Tick when overlapping a y-Tick and remove ticks near axis
    xTicks = ax.xaxis.get_major_ticks()
    yTicks = ax.yaxis.get_major_ticks()
    xMinTicks = ax.xaxis.get_minor_ticks()
    yMinTicks = ax.yaxis.get_minor_ticks()
    _,AxYPosTop = ax.transData.transform((0, yLimit[1]))
    _,AxYPosBot = ax.transData.transform((0, yLimit[0]))
    AxXPosRight,_ = ax.transData.transform((xLimit[1],0))
    AxXPosLeft,_ = ax.transData.transform((xLimit[0],0))

    for xT in xTicks+xMinTicks:
        xTickLoc = ax.transData.transform((xT.get_loc(), yLimit[0]))

        # Remove overlapping tick
        for yT in yTicks+yMinTicks:
            yTickLoc = ax.transData.transform((xLimit[0],yT.get_loc()))
            if (np.greater_equal(xTickLoc, yTickLoc - MajorTickLength).all() 
                and np.less_equal(xTickLoc,yTickLoc + MajorTickLength).all()):
                xT.tick1line.set_visible(False)
                xT.tick2line.set_visible(False)

            # Remove y tick near x-axis
            if ((yTickLoc[1] >= AxYPosBot - MajorTickWidth and yTickLoc[1] <= AxYPosBot + MajorTickWidth) 
                or (yTickLoc[1] >= AxYPosTop - MajorTickWidth and yTickLoc[1] <= AxYPosTop + MajorTickWidth)):
                yT.tick1line.set_visible(False)
                yT.tick2line.set_visible(False)

        # Remove x tick near y-axis
        if ((xTickLoc[0] >= AxXPosLeft - MajorTickWidth -5 and xTickLoc[0] <= AxXPosLeft + MajorTickWidth + 5) 
            or (xTickLoc[0] >= AxXPosRight - MajorTickWidth -5 and xTickLoc[0] <= AxXPosRight + MajorTickWidth + 5)):
            xT.tick1line.set_visible(False)
            xT.tick2line.set_visible(False)


    if LogScaleX and (LogScaleBaseX == np.e or LogScaleBaseX == math.e):
        def ticks(x, pos):
            return r'$\mathrm{e}^{'+'{:.0f}'.format(np.log(x))+'}$'
        ax.xaxis.set_major_formatter(ticker.FuncFormatter(ticks))

    if LogScaleY and (LogScaleBaseY == np.e or LogScaleBaseY == math.e):
        def ticks(y, pos):
            return r'$\mathrm{e}^{'+'{:.0f}'.format(np.log(y))+'}$'
        ax.yaxis.set_major_formatter(ticker.FuncFormatter(ticks))

    #for axis in [ax.xaxis, ax.yaxis]:
    #    axis.set_major_formatter(ticker.ScalarFormatter())

    #ax.yaxis.set_major_formatter(ticker.FuncFormatter(
        #lambda y,pos: ('{{:.{:1d}f}}'.format(int(np.maximum(-np.log10(y),0)))).format(y)))

    # Axe line width
    plt.setp(ax.spines.values(), linewidth=2)

    if type(Zoom) == list and len(Zoom) > 0:
        for s in Subs:
            s.set_xlim(Zoom[Subs.index(s)][0])
            s.set_ylim(Zoom[Subs.index(s)][1])
            if ZoomHelpLines:
                plt.fill_between(s.get_xlim(), s.get_ylim()[0], s.get_ylim()[1], facecolor='grey', alpha=0.5)
                for i in (0,1):
                    con = ConnectionPatch(xyA=(s.get_xlim()[i], s.get_ylim()[0]), coordsA=s.transData, 
                                        xyB=(s.get_xlim()[i], s.get_ylim()[0]), coordsB=ax.transData, color = 'grey', alpha = 0.5)
                    fig.add_artist(con)

            s.tick_params(axis="both",which="both",direction=TickDirection, top=True, right=True, labelsize = TickLabelSize*0.8)
            s.tick_params(axis="both",which="major",length=MajorTickLength*0.8, width=MajorTickWidth*0.8)
            s.tick_params(axis="both",which="minor",length=MinorTickLength*0.8, width=MinorTickWidth*0.8,
                            bottom=MinorTicksX, top=MinorTicksX, left=MinorTicksY, right=MinorTicksY)

            for side in s.spines.keys():  # 'top', 'bottom', 'left', 'right'
                s.spines[side].set_linewidth(2*0.8)

            s.grid(alpha = .2)
            if SciStyleXZoom and SciStyleXZoom[Subs.index(s)]:
                s.ticklabel_format(axis="x",style="sci", scilimits=(SciStyleXZoom[Subs.index(s)],
                                                                    SciStyleXZoom[Subs.index(s)]))
            if SciStyleYZoom and SciStyleYZoom[Subs.index(s)]: 
                s.ticklabel_format(axis="y",style="sci", scilimits=(SciStyleYZoom[Subs.index(s)], 
                                                                    SciStyleYZoom[Subs.index(s)]), useMathText=True)

            TickPeriodX = MajorTicksPeriodXZoom
            if type(MajorTicksPeriodXZoom) == list: TickPeriodX = MajorTicksPeriodXZoom[Subs.index(s)]
            if TickPeriodX: s.xaxis.set_major_locator(plt.MultipleLocator(TickPeriodX))
            TickPeriodY = MajorTicksPeriodYZoom
            if type(MajorTicksPeriodYZoom) == list: TickPeriodY = MajorTicksPeriodYZoom[Subs.index(s)]
            if TickPeriodY: s.xaxis.set_major_locator(plt.MultipleLocator(TickPeriodY))

            s.xaxis.offsetText.set_fontsize(TickLabelSize*0.8)
            s.yaxis.offsetText.set_fontsize(TickLabelSize*0.8)

    plt.tight_layout()
    
    return ScatterIDs

#def DeleteFits():
#    fig = plt.gcf()
#    ax = plt.gca()
#    if len(ax.lines) > 0:
#        ax.lines.clear()
#        Axes = fig.get_axes()
#        for a in Axes: a.lines.clear()
#    if ax.get_legend(): ax.get_legend().remove()
        
def AddFits(DataInfos, FitSettings):
    FitIDs = []
    FitsParams = {}
    NumFits = FitSettings["NumFits"]
    Underground, MeanLine = False, False
    xDatas, yDatas, xErrors, yErrors = PickData(DataInfos)
    for i in range(NumFits):
        FitArgs = {}
        for key, val in FitSettings.items():
            if val != None and key != "NumFits":
                if type(val) == list: FitArgs.update({key : val[i]})
                else: FitArgs.update({key : val})
        FitID, Underground, MeanLine, FitParams = ApplyFit(xDatas, yDatas, xErrors, yErrors, **FitArgs)
        FitIDs.append(FitID)
        FitsParams.update(FitParams)
    
    return FitIDs, Underground, MeanLine, FitsParams

def CreateLegend(PlotSettings, ScatterIDs, FitIDs, Underground, MeanLine):
    ax = plt.gca()
    LegendIDs = []

    # Default Settings:
    if not PlotSettings["LegendRelX"]: PlotSettings["LegendRelX"] = 0.8
    if not PlotSettings["LegendRelY"]: PlotSettings["LegendRelY"] = 0.8
    if not PlotSettings["LegendFontsize"]: PlotSettings["LegendFontsize"] = 20
    if not PlotSettings["LegendHandlelength"]: PlotSettings["LegendHandlelength"] = 3
    if not PlotSettings["LegendMarkerscale"]: PlotSettings["LegendMarkerscale"] = 2

    #LegendLabels = list(filter(None, PlotSettings["LegendLabels"]))
    LegendLabels = PlotSettings["LegendLabels"]

    for i, label in enumerate(PlotSettings["LegendOrder"]):
        if "Data" in label:
            LegendIDs.append(ScatterIDs[int(label.replace("Data ","")) - 1])
        elif "Fit" in label:
            LegendIDs.append(FitIDs[int(label.replace("Fit ","")) - 1])
        elif "Underground" in label:
            if not Underground: LegendLabels[i] = ""
            else: LegendIDs.append(Underground)
        elif "MeanLine" in label:
            if not MeanLine: LegendLabels[i] = ""
            else: LegendIDs.append(MeanLine)

    LegendLabels = list(filter(None, LegendLabels))

    if len(LegendLabels) == 0: return
    ax.legend(LegendIDs, LegendLabels, loc = "upper left", 
                bbox_to_anchor=(PlotSettings["LegendRelX"], PlotSettings["LegendRelY"]), 
                fontsize = PlotSettings["LegendFontsize"], handlelength=PlotSettings["LegendHandlelength"],
                markerscale=PlotSettings["LegendMarkerscale"])
    

#----------------------------------------------------------------------


# In[3]:


#--------------------------------Settings--------------------------------------
#--------------------------------------------------------------------------------


# In[4]:


#-----------------------------------Plot & Fit--------------------------------------

#FitIDs, Underground, MeanLine, FitsParams = AddFits(DataInfos, FitSettings)

#CreateLegend(PlotSettings, FitIDs, Underground, MeanLine)

#SaveParamsAsLatex(FitsParams)

#[Fit1, Fit2, Underground]
#[r"K$_\beta$-Linie", r"K$_\alpha$-Linie", "Untergrund"]
    
    
# find Fitfunction / manuel Fit
#A_find = 8
#SD_find = 0.3
#EV_find = 18.4
#Shape_find = -6
#SV_find = (A_find, SD_find, EV_find, Shape_find)
#x_find = np.linspace(*(17.8,18.6), 2000)
#y_find =SkewedGaussPDF(x_find,*SV_find)
#plt.plot(x_find, y_find, marker='None', linestyle='-', color='green')

#fig = plt.gcf()
#Axes = fig.get_axes()
#Axes[0].plot(x_find, y_find, marker='None', linestyle='-', color='green')
#print("Find Fit:")
#CalcFitEr(xData,yData,xError,yError,FallingExpFit,params=SV_find,pRes=False)


def CPlot(DataInfos, PlotSettings, FitFunctions, FitSettings):

    FitSettings["func"] = FitFunctions
    
    OutErr = 1
    
    ScatterIDs = PlotFigure(DataInfos, PlotSettings)

    FitIDs, Underground, MeanLine, FitsParams = AddFits(DataInfos, FitSettings)

    CreateLegend(PlotSettings, ScatterIDs, FitIDs, Underground, MeanLine)

    LocalDataPath = os.getenv('LOCALAPPDATA') + "\\Ezodox\\EzPlot"
    if not os.path.exists(LocalDataPath):
        os.makedirs(LocalDataPath)

    SaveParamsAsLatex(FitsParams, LocalDataPath)
    plt.savefig(LocalDataPath+"\\plot.png", dpi=50)

    return OutErr

def ShowPlot():
    plt.show()

