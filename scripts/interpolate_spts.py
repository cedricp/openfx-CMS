# training data found at : https://color-lab-eilat.github.io/Spectral-sensitivity-estimation-web/


import csv
import datetime
from scipy.interpolate import interp1d
import numpy as np
import matplotlib.pyplot as plt
import json
import uuid

model = "eos m"
specsens_csv = "data/Canon-EOS-M.csv"

template_doc = "canon_eos_5d_mark_ii_380_780_5.json"
datapath = "../resources/data/camera/"
new_name = datapath + "canon_" + model.replace(" ", "_") + "_380_780_5.json"
catalogid = "camera_canon_" + model.replace(" ", "_") + "_0.1.0"

with open(datapath+template_doc, "r") as jsonfile:
    camera_data = json.load(jsonfile)
    camera_data["header"]["model"] = model
    camera_data["header"]["catalog_number"] = catalogid
    camera_data["header"]["unique_identifier"] = str(uuid.uuid4())
    camera_data["header"]["document_creation_date"] = str(datetime.datetime.now())

    with open(specsens_csv, "r") as csvfile:
        reader = csv.reader(csvfile)
        data = list(reader)
        
        wl      = np.array([float(a[0]) for a in data[1:]])
        red     = np.array([float(a[1]) for a in data[1:]])
        green   = np.array([float(a[2]) for a in data[1:]])
        blue    = np.array([float(a[3]) for a in data[1:]])

        maxr = np.max(red)
        maxg = np.max(green)
        maxb = np.max(blue)
        scale = 1./np.max([maxr, maxg, maxb])

        red     *= scale
        green   *= scale
        blue    *= scale
        
        x_ = np.linspace(380, 780, 81)

        spline_red      = interp1d(wl, red, kind="quadratic", fill_value=(red[0]/2, red[-1]/2,), bounds_error=False)
        spline_green    = interp1d(wl, green, kind="quadratic", fill_value=(green[0]/2, green[-1]/2,), bounds_error=False)
        spline_blue     = interp1d(wl, blue, kind="quadratic", fill_value=(blue[0]/2, blue[-1]/2,), bounds_error=False)

        for wavelength in x_:
            camera_data["spectral_data"]["data"]["main"][str(int(wavelength))] = [float(spline_red(wavelength)), float(spline_green(wavelength)), float(spline_blue(wavelength))]

        with open(new_name, 'w', encoding='utf-8') as f:
            json.dump(camera_data, f, ensure_ascii=False, indent=4)

        plt.plot(x_, spline_red(x_))
        plt.plot(x_, spline_green(x_))
        plt.plot(x_, spline_blue(x_))
        plt.show()