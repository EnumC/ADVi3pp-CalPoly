Install homebrew (https://docs.brew.sh/Installation)
Install imagemagick (brew install imagemagick)

Install platformio: 
python3 -c "$(curl -fsSL https://raw.githubusercontent.com/platformio/platformio/master/scripts/get-platformio.py)"

Symlink binary (https://docs.platformio.org/en/latest//core/installation.html#method-2).

Install Macports.
Then, install dosfstools (sudo port install dosfstools)

Install Sketch.app (https://www.sketch.com/apps/#mac)
 - No need to register. Don't open the app after installing the app to Applications/


Run sudo ./create-release.sh 5.0.0 in Scripts. Replace 5.0.0 with any version number.
The output will be three directories up in a directory named "release".