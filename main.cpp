#include <QCoreApplication>
#include <QDebug>
#include <QDateTime>
#include <QFile>

const int TWO_MINUTES = 120;
const QTime START_TIME(0, 0, 0);
const QTime END_TIME(23, 59, 59);


struct TransportInfo {
    QDateTime date_time;
    QString id;
    int speed{};
};

struct DriveInfo {

    QString id;
    QTime travel_time;
    QTime parking_time;

    QDateTime prev_datetime;
    int prev_speed;

    QDateTime zero_speed_time;

    uint qHash(const DriveInfo &key);

    bool operator==(const DriveInfo &rhs) const {
        return id == rhs.id &&
               travel_time == rhs.travel_time &&
               parking_time == rhs.parking_time &&
               prev_datetime == rhs.prev_datetime &&
               prev_speed == rhs.prev_speed;
    }

    bool operator!=(const DriveInfo &rhs) const {
        return !(rhs == *this);
    }


    QString toString() const {

        return QString("id: %1\nвремя в пути: %2h\nвремя стоянки: %3h\n----\n")
                .arg(id)
                .arg(qRound(10.0 * (travel_time.hour() + (double) travel_time.minute() / 60)) / 10.0)
                .arg(qRound(10.0 * (parking_time.hour() + (double) parking_time.minute() / 60)) / 10.0);
    }

};

uint qHash(const DriveInfo &key) {
    return qHash(key.id);
}

//Функция, которая считывает .csv файл
QVector<TransportInfo> parseFile(const QString &filePath) {
    qDebug() << "Start parse file from" << filePath;

    QVector<TransportInfo> transport_vector;


    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file.";
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();

        QStringList fields = line.split(",");
        if (fields.size() < 3) {
            qWarning() << "Invalid string format";
            continue;
        }

        QString format = "yyyy-MM-dd hh:mm:ss";

        TransportInfo transport;
        transport.date_time = QDateTime::fromString(fields[0].mid(1, fields[0].length() - 2), "yyyy-MM-dd hh:mm:ss");
        transport.id = fields[1];
        transport.speed = fields[2].toInt();

        transport_vector.append(transport);
    }

    file.close();

    qDebug() << "End parse file";

    return transport_vector;
};

//Функция, которая считает время отсановки и время передвижения транспорта
QHash<QString, DriveInfo> calcDrivesStat(const QVector<TransportInfo> &transport_vector) {

    qDebug() << "Start calculation drive statistic";

    QHash<QString, DriveInfo> drive_hash;

    for (const auto &i: transport_vector) {
        DriveInfo drive;

        if (!drive_hash.contains(i.id)) {
            drive.id = i.id;
            drive.parking_time = QTime(0, 0, 0);
            drive.travel_time = QTime(0, 0, 0);
            // учет времени от начала в 00:00:00 до первых данных
            auto time_difference = START_TIME.secsTo(i.date_time.time());
            if (time_difference > TWO_MINUTES) {
                drive.parking_time = drive.parking_time.addSecs(time_difference);
            } else {
                drive.travel_time = drive.travel_time.addSecs(time_difference);
            }

            drive.prev_datetime = i.date_time;
            drive.prev_speed = i.speed;
            drive.zero_speed_time = i.date_time;
            drive_hash.insert(i.id, drive);
        } else {

            drive = drive_hash[i.id];

            //двигвлся - стоит
            if (i.speed == 0 && drive.prev_speed != 0) {
                auto time_difference = drive.prev_datetime.time().secsTo(i.date_time.time());
                drive.travel_time = drive.travel_time.addSecs(time_difference);

                drive.zero_speed_time = i.date_time;
                drive.prev_speed = i.speed;
                drive.prev_datetime = i.date_time;

                drive_hash.insert(i.id, drive);
                continue;
            }

            //стоял - стоит
            if (i.speed == 0 && drive.prev_speed == 0) {
                drive.prev_speed = i.speed;
                drive.prev_datetime = i.date_time;

                drive_hash.insert(i.id, drive);
                continue;
            }

            //стоял - двигается
            if (i.speed != 0 && drive.prev_speed == 0) {
                auto zero_time_difference = drive.zero_speed_time.time().secsTo(i.date_time.time());

                if (zero_time_difference > TWO_MINUTES) {
                    drive.parking_time = drive.parking_time.addSecs(zero_time_difference);
                } else {
                    drive.travel_time = drive.travel_time.addSecs(zero_time_difference);
                }

                drive.prev_speed = i.speed;
                drive.prev_datetime = i.date_time;

                drive_hash.insert(i.id, drive);
                continue;
            }

            //двигался - двигается
            if (i.speed != 0 && drive.prev_speed != 0) {
                drive.travel_time = drive.travel_time.addSecs(drive.prev_datetime.time().secsTo(i.date_time.time()));
                drive.prev_speed = i.speed;
                drive.prev_datetime = i.date_time;

                drive_hash.insert(i.id, drive);
                continue;
            }

        }


    }

    for (auto &di: drive_hash) {
       //учет времени в том случае, когда зафиксировались последние данные со нулевой скоростью
        if (di.zero_speed_time != di.prev_datetime && di.prev_speed == 0) {
            auto zero_time_difference = di.zero_speed_time.time().secsTo(di.prev_datetime.time());

            if (zero_time_difference > TWO_MINUTES) {
                di.parking_time = di.parking_time.addSecs(zero_time_difference);
            } else {
                di.travel_time = di.travel_time.addSecs(zero_time_difference);
            }

        }
        // учет времени оставшегося до 23:59:59
        auto time_difference = di.prev_datetime.time().secsTo(END_TIME);
        if (time_difference > TWO_MINUTES) {
            di.parking_time = di.parking_time.addSecs(time_difference);
        } else {
            di.travel_time = di.travel_time.addSecs(time_difference);
        }

    }

    qDebug() << "End calculation drive statistic";

    return drive_hash;
};

//Функция, которая записывает статистику в .txt файл
void writeQHashToFile(const QHash<QString, DriveInfo> &hash, const QString &fileName) {
    qDebug() << "Start write output into" << fileName;
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Не удалось открыть файл для записи:" << fileName;
        return;
    }

    QTextStream out(&file);
    QHash<QString, DriveInfo>::const_iterator it;
    out << "----\n";
    for (it = hash.constBegin(); it != hash.constEnd(); ++it) {
        out << it.value().toString();
    }

    file.close();
    qDebug() << "End write output";
}


int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QStringList arguments = QCoreApplication::arguments();

    const QString &pathIn = arguments.at(1);

    const QString &pathOut = arguments.at(2);

    qDebug() << "Start program";

    QVector<TransportInfo> test_transport = parseFile(pathIn);

    QHash<QString, DriveInfo> test_hash = calcDrivesStat(test_transport);

    writeQHashToFile(test_hash, pathOut);

    qDebug() << "End program";
    return 0;
}
