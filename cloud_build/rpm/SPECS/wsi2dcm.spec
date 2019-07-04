Name:           wsi2dcm
Version:
Release:        1
Summary:        Software library to create DICOM files from whole slide imaging files
Packager:       Google LLC
Group:          System Environment/libraries
License:        Apache License, Version 2.0
Requires:       openslide >= 3.4.0, jsoncpp

%description
wsi2dcm is a library to create DICOM files from whole slide imaging files

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%files
%defattr(-,root,root)
/usr/lib64/libwsi2dcm.so
/usr/bin/wsi2dcm
