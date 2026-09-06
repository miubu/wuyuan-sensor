#include <Arduino.h>
#include <SPI.h>
#include <PN5180.h>
#include <PN5180ISO15693.h>

// ======================================================
// ESP32-C3 SuperMini -> PN5180
// ======================================================

#define PN5180_NSS   3
#define PN5180_BUSY  5
#define PN5180_RST   4

#define PN5180_SCK   6
#define PN5180_MISO  7
#define PN5180_MOSI  10


// ======================================================
// 当前实际硬件
//
// ADC1 -> 应变片 -> SVSS
// ADC2 -> 200k参考电阻 -> SVSS
// SVSS -> 1uF -> VSS/GND
// ======================================================

const float R_REF_OHM = 200000.0f;


// ======================================================
// ADC 配置
//
// 0x18
//
// bits1:0 = 00  -> PGA ×1
// bit2    = 0   -> CIC
// bits5:3 = 011 -> Decimation 256
// bit6    = 0   -> SVSS reference
//
// 前面的 0x19 是 PGA ×2，
// 200k 参考电阻容易饱和，所以改成 ×1。
// ======================================================

const uint8_t ADC_CONFIG = 0x18;


PN5180ISO15693 nfc(
    PN5180_NSS,
    PN5180_BUSY,
    PN5180_RST
);

uint8_t uid[8];


// ======================================================
// 打印错误
// ======================================================

void printError(ISO15693ErrorCode rc)
{
    Serial.print("Error code = ");

    if ((int)rc < 0)
    {
        Serial.println((int)rc);
    }
    else
    {
        Serial.print("0x");

        if ((int)rc < 0x10)
            Serial.print("0");

        Serial.println((int)rc, HEX);
    }
}


// ======================================================
// 打印 UID
// ======================================================

void printUID()
{
    Serial.print("UID = ");

    for (int i = 7; i >= 0; i--)
    {
        if (uid[i] < 0x10)
            Serial.print("0");

        Serial.print(uid[i], HEX);

        if (i != 0)
            Serial.print(":");
    }

    Serial.println();
}


// ======================================================
// 打印 Block
// ======================================================

void printBlock(
    uint8_t blockNo,
    const uint8_t *data,
    uint8_t size
)
{
    Serial.print("Block ");

    if (blockNo < 10)
        Serial.print(" ");

    Serial.print(blockNo);
    Serial.print(" : ");

    for (uint8_t i = 0; i < size; i++)
    {
        if (data[i] < 0x10)
            Serial.print("0");

        Serial.print(data[i], HEX);
        Serial.print(" ");
    }

    Serial.println();
}


// ======================================================
// 读 Block
// ======================================================

bool readBlock(
    uint8_t blockNo,
    uint8_t *data,
    uint8_t size
)
{
    ISO15693ErrorCode rc =
        nfc.readSingleBlock(
            uid,
            blockNo,
            data,
            size
        );

    if (rc != ISO15693_EC_OK)
    {
        Serial.print("Read Block ");
        Serial.print(blockNo);
        Serial.print(" failed. ");

        printError(rc);

        return false;
    }

    return true;
}


// ======================================================
// 写 Block
// ======================================================

bool writeBlock(
    uint8_t blockNo,
    uint8_t *data,
    uint8_t size
)
{
    ISO15693ErrorCode rc =
        nfc.writeSingleBlock(
            uid,
            blockNo,
            data,
            size
        );

    if (rc != ISO15693_EC_OK)
    {
        Serial.print("Write Block ");
        Serial.print(blockNo);
        Serial.print(" failed. ");

        printError(rc);

        return false;
    }

    return true;
}


// ======================================================
// 配置 ADC1 / ADC2
// ======================================================

bool configureADC()
{
    uint8_t block2[8];

    if (!readBlock(2, block2, 8))
        return false;


    Serial.println();
    Serial.println("Original Block 2:");

    printBlock(2, block2, 8);


    // ADC1 = 应变片
    block2[0] = ADC_CONFIG;

    // ADC2 = 200k 参考电阻
    block2[1] = ADC_CONFIG;


    Serial.println("New Block 2:");

    printBlock(2, block2, 8);


    if (!writeBlock(2, block2, 8))
    {
        Serial.println("Write Block 2 failed!");
        return false;
    }


    // 写后验证
    uint8_t verify[8];

    if (!readBlock(2, verify, 8))
        return false;


    Serial.println("Verify Block 2:");

    printBlock(2, verify, 8);


    if (verify[0] != ADC_CONFIG ||
        verify[1] != ADC_CONFIG)
    {
        Serial.println("Block 2 verify FAILED!");
        return false;
    }


    Serial.println("ADC configuration OK.");

    return true;
}


// ======================================================
// 启动一次 ADC1 + ADC2 采样
// ======================================================

bool startSampling()
{
    uint8_t block0[8] =
    {
        0x01,   // Byte0: Start = 1

        0x00,   // Byte1: Status

        0x03,   // Byte2:
                // bit0 = ADC1
                // bit1 = ADC2

        0x03,   // Byte3: Frequency

        0x01,   // Byte4: 1 pass

        0x01,   // Byte5: 不额外平均

        0x00,   // Byte6

        0x40    // Byte7:
                // UsingThermistor = 1
                // 开启 ADC1/ADC2 电阻偏置电流
    };


    Serial.println();
    Serial.println("Start configuration:");

    printBlock(0, block0, 8);


    if (!writeBlock(0, block0, 8))
    {
        Serial.println("Write Block 0 failed!");
        return false;
    }


    Serial.println("Sampling started.");

    return true;
}


// ======================================================
// 等待采样完成
// ======================================================

bool waitSampling()
{
    uint8_t block0[8];


    Serial.println();
    Serial.println("Waiting sample...");


    // CIC256 + 两个通道
    // 先等一会，避免马上读到旧状态
    delay(600);


    // 再最多等待约4秒
    for (int i = 0; i < 200; i++)
    {
        if (!readBlock(0, block0, 8))
        {
            delay(20);
            continue;
        }


        uint8_t state =
            block0[1] & 0x03;


        if (state == 0)
        {
            // Idle
        }
        else if (state == 1)
        {
            Serial.println("State = Sampling");
        }
        else if (state == 2)
        {
            Serial.println("State = Data available");

            return true;
        }
        else if (state == 3)
        {
            Serial.println("State = ERROR");

            Serial.print("Status = 0x");
            Serial.println(block0[1], HEX);

            return false;
        }


        delay(20);
    }


    Serial.println("Sampling timeout!");

    return false;
}


// ======================================================
// 读取结果
// ======================================================

bool readResult()
{
    uint8_t block9[8];


    if (!readBlock(9, block9, 8))
        return false;


    Serial.println();
    Serial.println("---------- RAW DATA ----------");

    printBlock(9, block9, 8);


    // --------------------------------------------------
    // Block 9:
    //
    // Byte0~1 -> ADC1
    // Byte2~3 -> ADC2
    //
    // little endian
    // --------------------------------------------------

    uint16_t adc1 =
        ((uint16_t)block9[1] << 8) |
        block9[0];


    uint16_t adc2 =
        ((uint16_t)block9[3] << 8) |
        block9[2];


    adc1 &= 0x3FFF;
    adc2 &= 0x3FFF;


    // 当前实际接线
    uint16_t strainRaw = adc1;
    uint16_t refRaw    = adc2;


    Serial.println();

    Serial.print("ADC1 strain RAW     = ");
    Serial.println(strainRaw);

    Serial.print("ADC2 200k ref RAW   = ");
    Serial.println(refRaw);


    Serial.print("ADC1 HEX = 0x");
    Serial.println(strainRaw, HEX);

    Serial.print("ADC2 HEX = 0x");
    Serial.println(refRaw, HEX);


    // --------------------------------------------------
    // 数据有效性检查
    // --------------------------------------------------

    bool valid = true;


    if (strainRaw == 0)
    {
        Serial.println(
            "WARNING: ADC1 = 0"
        );

        valid = false;
    }


    if (refRaw == 0)
    {
        Serial.println(
            "WARNING: ADC2 reference = 0"
        );

        valid = false;
    }


    if (strainRaw == 0x3FFF)
    {
        Serial.println(
            "WARNING: ADC1 saturated!"
        );

        valid = false;
    }


    if (refRaw == 0x3FFF)
    {
        Serial.println(
            "WARNING: ADC2 saturated!"
        );

        valid = false;
    }


    if (!valid)
    {
        Serial.println();

        Serial.println(
            "ADC invalid. "
            "Resistance calculation skipped."
        );

        return false;
    }


    // ==================================================
    // 电阻计算
    //
    // 当前接法：
    //
    // ADC1 = 应变片
    // ADC2 = 200k参考
    //
    // 所以：
    //
    // Rstrain =
    // Rref × ADC1 / ADC2
    // ==================================================

    float ratio =
        (float)strainRaw /
        (float)refRaw;


    float resistance =
        R_REF_OHM * ratio;


    Serial.println();
    Serial.println("---------- RESULT ----------");


    Serial.print("Reference resistance = ");
    Serial.print(R_REF_OHM, 1);
    Serial.println(" ohm");


    Serial.print("ADC1 / ADC2          = ");
    Serial.println(ratio, 6);


    Serial.print("Strain resistance    = ");
    Serial.print(resistance, 2);
    Serial.println(" ohm");


    Serial.print("Strain resistance    = ");
    Serial.print(resistance / 1000.0f, 3);
    Serial.println(" kOhm");


    return true;
}


// ======================================================
// 一次完整测量
// ======================================================

void measureOnce()
{
    Serial.println();
    Serial.println(
        "================================"
    );

    Serial.println(
        "Start resistance measurement"
    );

    Serial.println(
        "ADC1 = strain gauge"
    );

    Serial.println(
        "ADC2 = 200k reference"
    );

    Serial.println(
        "Gain = x1"
    );

    Serial.println(
        "================================"
    );


    if (!configureADC())
        return;


    if (!startSampling())
        return;


    if (!waitSampling())
        return;


    readResult();


    Serial.println(
        "================================"
    );
}


// ======================================================
// SETUP
// ======================================================

void setup()
{
    Serial.begin(115200);

    delay(1500);


    Serial.println();
    Serial.println(
        "================================"
    );

    Serial.println(
        "RF430FRL152H Resistance Reader"
    );

    Serial.println(
        "ADC1 = strain gauge"
    );

    Serial.println(
        "ADC2 = 200k reference"
    );

    Serial.println(
        "ADC_CONFIG = 0x18"
    );

    Serial.println(
        "PGA = x1"
    );

    Serial.println(
        "================================"
    );


    pinMode(PN5180_NSS, OUTPUT);
    pinMode(PN5180_BUSY, INPUT);
    pinMode(PN5180_RST, OUTPUT);

    digitalWrite(PN5180_NSS, HIGH);
    digitalWrite(PN5180_RST, HIGH);


    SPI.begin(
        PN5180_SCK,
        PN5180_MISO,
        PN5180_MOSI,
        PN5180_NSS
    );


    delay(50);


    Serial.println("Reset PN5180...");

    nfc.reset();

    delay(20);


    Serial.println("Setup ISO15693 RF...");


    if (!nfc.setupRF())
    {
        Serial.println(
            "PN5180 setupRF FAILED!"
        );

        return;
    }


    Serial.println("PN5180 ready.");
    Serial.println("Waiting for RF430...");
}


// ======================================================
// LOOP
// ======================================================

void loop()
{
    ISO15693ErrorCode rc =
        nfc.getInventory(uid);


    if (rc != ISO15693_EC_OK)
    {
        Serial.println(
            "Waiting for RF430..."
        );

        delay(500);

        return;
    }


    Serial.println();
    Serial.println("------------------------");

    Serial.println("RF430 detected!");

    printUID();


    uint8_t blockSize = 0;
    uint8_t numBlocks = 0;


    rc = nfc.getSystemInfo(
        uid,
        &blockSize,
        &numBlocks
    );


    if (rc != ISO15693_EC_OK)
    {
        Serial.print(
            "getSystemInfo failed. "
        );

        printError(rc);

        delay(1000);

        return;
    }


    Serial.print("Block size = ");
    Serial.println(blockSize);

    Serial.print("Blocks     = ");
    Serial.println(numBlocks);


    if (blockSize != 8)
    {
        Serial.println(
            "ERROR: Expected 8-byte blocks."
        );

        delay(2000);

        return;
    }


    measureOnce();


    delay(2000);
}