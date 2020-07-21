#include "uohues.h"
#include "EasyBMP/EasyBMP.h"        // for the bitmaps (http://easybmp.sourceforge.net/)
#include "img2dds/ImageBuilder.hh"  // for the DDS (https://github.com/ducakar/img2dds)
#include "uoppackage/uoppackage.h"  // pack everything to the UOP file
#include <fstream>
#include <iostream>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QImage>
#include <QRgb>


/* -- Globals -- */

const char kUnkddsName[] = "0xFA5C6A1BC0D8B01B_.dds";
const unsigned long long kUnkddsHash = 0xFA5C6A1BC0D8B01B;
const QString kOutDir = "huesmul2uop_temp";


/* -- Program "skeleton" -- */

bool doWork();

int main()
{
    doWork();

    std::cout << "\nPress a key to exit." << std::endl;
    std::cin.get();

    return 0;
}


/* -- Utility functions -- */

bool checkRemoveFile(const QString& filePath)
{
    if (QFile::exists(filePath) && !QFile::remove(filePath))
    {
        std::cout << "Can't delete " << filePath.toStdString() << "! Aborting this task." << std::endl;
        return false;
    }
    return true;
}


/* -- Image-related utility functions -- */

// from: https://stackoverflow.com/questions/3903223/qt4-how-to-blur-qpixmap-image
QImage customBlur(const QImage& image, int radius, const QRect& rect, bool alphaOnly = false)
{
    constexpr int tab[] = { 14, 10, 8, 6, 5, 5, 4, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2 };
    int alpha = (radius < 1)  ? 16 : (radius > 17) ? 1 : tab[radius-1];

    QImage::Format oFormat = image.format();
    QImage result = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int r1 = rect.top();
    const int r2 = rect.bottom();
    const int c1 = rect.left();
    const int c2 = rect.right();

    const int bpl = result.bytesPerLine();
    int rgba[4];
    unsigned char* p;

    int i1 = 0;
    int i2 = 3;

    if (alphaOnly)
        i1 = i2 = (QSysInfo::ByteOrder == QSysInfo::BigEndian ? 0 : 3);

    for (int col = c1; col <= c2; ++col) {
        p = result.scanLine(r1) + col * 4;
        for (int i = i1; i <= i2; ++i)
            rgba[i] = p[i] << 4;

        p += bpl;
        for (int j = r1; j < r2; ++j, p += bpl)
            for (int i = i1; i <= i2; ++i)
                p[i] = uchar((rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4);
    }

    for (int row = r1; row <= r2; ++row) {
        p = result.scanLine(row) + c1 * 4;
        for (int i = i1; i <= i2; ++i)
            rgba[i] = p[i] << 4;

        p += 4;
        for (int j = c1; j < c2; ++j, p += 4)
            for (int i = i1; i <= i2; ++i)
                p[i] = uchar((rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4);
    }

    for (int col = c1; col <= c2; ++col) {
        p = result.scanLine(r2) + col * 4;
        for (int i = i1; i <= i2; ++i)
            rgba[i] = p[i] << 4;

        p -= bpl;
        for (int j = r1; j < r2; ++j, p -= bpl)
            for (int i = i1; i <= i2; ++i)
                p[i] = uchar((rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4);
    }

    for (int row = r1; row <= r2; ++row) {
        p = result.scanLine(row) + c2 * 4;
        for (int i = i1; i <= i2; ++i)
            rgba[i] = p[i] << 4;

        p -= 4;
        for (int j = c1; j < c2; ++j, p -= 4)
            for (int i = i1; i <= i2; ++i)
                p[i] = uchar((rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4);
    }

    result = result.convertToFormat(oFormat);
    return result;
}


/* -- Utility function to convert PNG to UNCOMPRESSED DDS -- */

bool convertPNGtoDDS(const std::string& inPathPNG, const std::string& outPathDDS )
{
    constexpr int ddsOptions = 0; // uncompressed: best quality
    //constexpr int ddsOptions = ImageBuilder::COMPRESSION_BIT; //DXT5 (compressed): worst colors, totally unaccurate for very small pictures
    constexpr double scale = 1.0;

    ImageData image = ImageBuilder::loadImage(inPathPNG.c_str());
    /*
    if (image.flags & ImageData::NORMAL_BIT)
    {
        ddsOptions |= ImageBuilder::NORMAL_MAP_BIT;
        ddsOptions &= ~(ImageBuilder::YYYX_BIT | ImageBuilder::ZYZX_BIT);
    }
    */

    if (image.isEmpty())
    {
        std::cout << "\n\tPNGtoDDS: Failed to open image \"" << inPathPNG << "\"." << std::endl;
        return false;
    }
    else if (!ImageBuilder::createDDS(&image, 1, ddsOptions, scale, outPathDDS.c_str()))
    {
        std::cout << "\n\tPNGtoDDS: failed to generate DDS \"" << outPathDDS << "\"." << std::endl;
        return false;
    }

    return true;
}


/* -- Start of image-processing functions -- */

static constexpr int kColorWidth = 8;    // 32 blocks, one for each color, and each one wide 8 pixels
static constexpr int kColorBlocks = 32;  // bmpWidth/colorWidth = 256 / 8 = 32


void createBitmaps(const UOHues& huesMul)   // the client uses them to hue the animations and the gump art and for the hue picker hardcoded dialog
{
    //-- create the bitmaps inside data/definitions/hues

    // They are BMP3, uncompressed, 8 bits for Red, 8 for Green, 8 for Blue, plus Alpha.
    //  Neither Qt and SOIL2 support bitmaps with alpha, but EasyBMP does!

    // Modified EasyBMP WriteToFile method to write these values in the header, which are the ones used in the original bitmaps
    //  (even if, the client appears to not care if the header values are the EasyBMP default ones)
    // (0x22) biSizeImage = 0 (the image size. This is the size of the raw bitmap data; a dummy 0 can be given for BI_RGB bitmaps (which is our case, because BI_RGB = no compression))
    // Use SetDPI method to overwrite these values (WriteToFile considered 0 as an invalid value, and used the default one, which is 96):
    // (0x26) horizontal res (ppi) = 0
    // (0x2A) vertical   res (ppi) = 0

    std::cout << "Creating the bitmaps... ";


    for (int iHue = 1; iHue <= 3000; ++iHue)
    {
        UOHueEntry hueEntry = huesMul.getHueEntry(iHue);

        // First line (y = 0): full hue range (each hue color [of the 32] is drawn for 8 pixels here, then the whole line is blurred) (used in game to actually hue some stuff)
        QImage firstLineImg(256, 1, QImage::Format_ARGB32);
        for (int iColor = 0; iColor < kColorBlocks; ++iColor)
        {
            ARGB32 color32 = convert_ARGB16_to_ARGB32_exact(hueEntry.getColor(iColor));
            color32.setA(255);
            for (int x = 0; x < kColorWidth; ++x)
            {
                int curX = (iColor * kColorWidth) + x;
                firstLineImg.setPixel(curX, 0, color32.getVal());
            }
        }
        firstLineImg = customBlur(firstLineImg, 4, firstLineImg.rect(), false);

        // Second line (y = 1): it's just the color[31] of the hue repeated for all the line (used for the in game hue picker)
        ARGB32 secondlnColor32 = convert_ARGB16_to_ARGB32_exact(hueEntry.getColor(31));
        RGBApixel secondlnPix;
        secondlnPix.Red = secondlnColor32.getR();
        secondlnPix.Green = secondlnColor32.getG();
        secondlnPix.Blue = secondlnColor32.getB();
        secondlnPix.Alpha = 255;

        // Build the bitmap
        BMP bitmap;
        bitmap.SetSize(256, 2);
        bitmap.SetBitDepth(32);
        bitmap.SetDPI(0, 0);
        for (int iPixel = 0; iPixel < kColorBlocks * kColorWidth; ++iPixel)
        {
            QRgb qPix = firstLineImg.pixel(iPixel, 0);
            RGBApixel firstlnPix;
            firstlnPix.Red     = uchar(qRed  (qPix));
            firstlnPix.Green   = uchar(qGreen(qPix));
            firstlnPix.Blue    = uchar(qBlue (qPix));
            firstlnPix.Alpha   = 255;

            bitmap.SetPixel(iPixel, 0, firstlnPix);
            bitmap.SetPixel(iPixel, 1, secondlnPix);
        }

        QString outFileName = QString(kOutDir+"/data/definitions/hues/hue%1.bmp").arg(iHue,4,10,QChar('0'));
        if (!checkRemoveFile(outFileName))
            return;
        bitmap.WriteToFile(outFileName.toStdString().c_str());
    }

    std::cout << "done." << std::endl;
}


void createHuesDDS(const UOHues& huesMul)   // the client uses it to color the art tiles
{
    //-- create the file build/hues/hues.dds

    std::cout << "Generating hues.dds data... ";
    QImage huesdds(1024, 1024, QImage::Format_ARGB32);
    huesdds.fill(0);
    // first hue is 0: transparent. the first real hue is 1 (which is hue entry 0 in hues.mul)
    static constexpr int columnWidth = 256;     // which is colorWidth * colorBlocks
    int iHue = 1;
    int iRow = 1;
    for (int iColumn = 0; iColumn < 3; ++iColumn)
    {
        for (; iRow < 1024; ++iRow)
        {
            if (iHue > 3000)
                break;

            UOHueEntry hueEntry = huesMul.getHueEntry(iHue);

            // generate each row as if it was a separate image, so that i can blur the colors by blurring the whole image
            //  (tbh this is required only by the qtBlur function, but not by customblur)
            QImage rowImg(kColorBlocks*kColorWidth, 1, QImage::Format_ARGB32);
            for (int iColor = 0; iColor < kColorBlocks; ++iColor)
            {
                ARGB32 color32 = convert_ARGB16_to_ARGB32_exact(hueEntry.getColor(iColor));
                color32.setA(255);
                uint rawColor = color32.getVal();
                for (int w = 0; w < kColorWidth; ++w)
                {
                    int curX = (iColor * kColorWidth) + w;
                    rowImg.setPixel(curX, 0, rawColor);
                }
            }
            rowImg = customBlur(rowImg, 6, rowImg.rect(), false);

            // copy the blurred row to the main one
            for (int xRow = 0; xRow < kColorBlocks * kColorWidth; ++xRow)
            {
                int curMainX = (iColumn*columnWidth) + xRow;
                QRgb rowPixel = rowImg.pixel(xRow, 0);
                huesdds.setPixel(curMainX, iRow, rowPixel);
            }

            ++iHue;
        }
        iRow = 0;
    }
    std::cout << "done." << std::endl;


    std::cout << "Saving hues.dds... ";

    QString outFileNamePNG = kOutDir+"/huesdds.png";
    if (!checkRemoveFile(outFileNamePNG))
        return;
    if (!huesdds.save(outFileNamePNG,"PNG",100))
    {
        std::cout << "Error saving the intermediate PNG file. Aborting computation for this file." << std::endl;
        return;
    }

    QString outFileName = kOutDir+"/build/hues/hues.dds";
    if (!checkRemoveFile(outFileName))
        return;
    if (convertPNGtoDDS(outFileNamePNG.toStdString(), outFileName.toStdString()))
        std::cout << "done." << std::endl;
}


void createUnkDDS(const UOHues& huesMul)    // unknown use...
{
    //-- create dds file with hash 0xFA5C6A1BC0D8B01B (name unknown)

    std::cout << "Generating the unknown dds data... ";
    QImage unkdds(64, 64, QImage::Format_ARGB32);
    unkdds.fill(0);
    bool stop = false;
    int iHue = 1;
    for (int x = 0; (x < 64) && !stop; ++x)
    {
        // the first pixel of the grid is transparent, the second is the hue index 0 (black)
        for (int y = (x==0) ? 1 : 0; (y < 64) && !stop; ++y)
        {
            UOHueEntry hueEntry = huesMul.getHueEntry(iHue);
            // Use mean color. It results in a too dark palette.
            /*
            uint meanR, meanG, meanB;
            meanR = meanG = meanB = 0;
            for (int iColor = 0; iColor < colorBlocks; ++iColor)
            {
                ARGB32 color32 = hueEntry.getColor(iColor);
                meanR += color32.getR();
                meanG += color32.getG();
                meanB += color32.getB();
            }
            meanR /= colorBlocks;
            meanG /= colorBlocks;
            meanB /= colorBlocks;
            unkdds.setPixel(x, y, qRgba(meanR, meanG, meanB, 255));
            */

            // Use a specific shade of the hue.
            ARGB32 color32 = convert_ARGB16_to_ARGB32_exact(hueEntry.getColor(31));
            //ARGB32 color32 = ARGB32(hueEntry.getColor(21));
            //color32.adjustSaturation(120);
            color32.setA(255);
            unkdds.setPixel(x, y, color32.getVal());

            ++iHue;
            if (iHue > 3000)
                stop = true;
        }
    }
    std::cout << "done." << std::endl;


    std::cout << "Saving the unknown dds... ";

    QString outFileNamePNG = QString(kOutDir)+"/unkdds.png";
    if (!checkRemoveFile(outFileNamePNG))
        return;
    if (!unkdds.save(outFileNamePNG,"PNG",100))
    {
        std::cout << "Error saving the intermediate PNG file. Aborting computation for this file." << std::endl;
        return;
    }

    QString outFileName = QString(kOutDir)+'/'+kUnkddsName;
    if (!checkRemoveFile(outFileName))
        return;
    if (convertPNGtoDDS(outFileNamePNG.toStdString(), outFileName.toStdString()))
        std::cout << "done." << std::endl;
}


void createHuenamesCSV(const UOHues& huesMul)
{
    //-- create data/definitions/hues/huenames.csv

    std::cout << "Creating huenames.csv... ";

    QString outFileName(kOutDir + "/data/definitions/hues/huenames.csv");
    if (!checkRemoveFile(outFileName))
        return;

    QFile csvFile(outFileName);
    if (csvFile.open(QIODevice::WriteOnly))
    {
        QTextStream csvStream(&csvFile);
        for (int i = 1; i <= 3000; ++i)
        {
            UOHueEntry entry = huesMul.getHueEntry(i);
            csvStream << i;
            //if (!entry.getName().empty())
            //{
                csvStream << ",";
                csvStream << entry.getName().c_str();
            //}
            csvStream << "\r\n";
        }
        csvFile.close();
        std::cout << "done." << std::endl;
    }
    else
        std::cout << "file I/O error." << std::endl;
}



/* -- Execute the work -- */

bool doWork()
{
    //std::cout << "Executing from: " << QDir::currentPath().toStdString() << std::endl;
    std::cout << "Loading hues.mul... ";
    std::string path_hues = "hues.mul";
    std::ifstream fs_hues;
    fs_hues.open(path_hues, std::ios::in | std::ios::binary);
    if (!fs_hues.is_open())
    {
        std::cout << "error." << std::endl;
        return false;
    }
    fs_hues.close();
    UOHues huesMul(path_hues);
    std::cout << "done." << std::endl;

    QDir outQDir(kOutDir);
    outQDir.removeRecursively();
    outQDir.mkpath("data/definitions/hues");
    outQDir.mkpath("build/hues");

    // Create the files to be stored inside the uop
    ImageBuilder::init();
    createBitmaps(huesMul);
    createHuesDDS(huesMul);
    createUnkDDS(huesMul);
    createHuenamesCSV(huesMul);

    std::cout << "Packing into hues.uop... ";

    std::string outDirStd = kOutDir.toStdString() + '/';
    std::string fileNameInternal;

    // The client doesn't appear to care if the uop version is 4 or 5, but the original hues.uop uses the version 4, so let's use that
    uopp::UOPPackage uop(4);
    uopp::UOPError uopErr;

    auto checkUopErr = [&uopErr](bool endl = false)
    {
        if (uopErr.errorOccurred())
        {
            for (const std::string& errStr : uopErr.getErrorQueue())
                std::cout << "UOP Error: " << errStr << std::endl;
            if (endl)
                std::cout << std::endl;
            uopErr.clear();
        }
    };

    fileNameInternal = kUnkddsName;
    uop.addFile(outDirStd + fileNameInternal, kUnkddsHash, uopp::CompressionFlag::ZLib, true, &uopErr);
    checkUopErr();

    fileNameInternal = "build/hues/hues.dds";
    uop.addFile(outDirStd + fileNameInternal, fileNameInternal, uopp::CompressionFlag::ZLib, true, &uopErr);
    checkUopErr();

    for (int iHue = 1; iHue <= 3000; ++iHue)
    {
        std::string strHue = std::to_string(iHue);
        fileNameInternal = "data/definitions/hues/hue" + std::string(4 - uint(strHue.length()), '0') + strHue + ".bmp";
        uop.addFile(outDirStd + fileNameInternal, fileNameInternal, uopp::CompressionFlag::ZLib, true, &uopErr);
    }
    checkUopErr();

    fileNameInternal = "data/definitions/hues/huenames.csv";
    uop.addFile(outDirStd + fileNameInternal, fileNameInternal, uopp::CompressionFlag::ZLib, true, &uopErr);
    checkUopErr();

    uop.finalizeAndSave("hues.uop");
    checkUopErr(true);

    std::cout << "done." << std::endl;

    // Done
    std::cout << "Cleaning up temporary directory... ";
    outQDir.removeRecursively();
    std::cout << "done." << std::endl;

    return true;
}
