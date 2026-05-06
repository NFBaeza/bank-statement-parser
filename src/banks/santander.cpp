#include "banks/santander.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QRegularExpression>
#include <QTime>

namespace {

// Santander rows always start with "DD/MM ".
const QRegularExpression &rowStartRx()
{
    static const QRegularExpression rx(
        QStringLiteral(R"(^\s*\d{1,2}/\d{1,2}\b)"));
    return rx;
}

// Layout: <DD/MM> <description...> <amount> <balance>.
// Two trailing numbers — the last one is the running balance, the second-
// to-last is the posted amount of this movement.
const QRegularExpression &rowRx()
{
    static const QRegularExpression rx(
        QStringLiteral(
            R"(^(?<date>\d{1,2}/\d{1,2})\s+)"
            R"((?<desc>.+?)\s+)"
            R"((?<amount>-?[\d\.,]+)\s+)"
            R"((?<balance>-?[\d\.,]+)\s*$)"
        ));
    return rx;
}

// Statement period appears as "<from> <to>" — both DD/MM/YYYY — somewhere on
// page 1 (e.g. "36 28/06/2024 31/07/2024 1 de 1"). Use the FROM year so a
// row dated "01/07" gets the right year even when the period crosses Dec→Jan.
int statementYear(const QStringList &pages)
{
    static const QRegularExpression periodRx(
        QStringLiteral(R"(\b\d{1,2}/\d{1,2}/(?<from>\d{4})\s+\d{1,2}/\d{1,2}/(?<to>\d{4})\b)"));
    for (const QString &p : pages) {
        const auto m = periodRx.match(p);
        if (m.hasMatch()) return m.captured("from").toInt();
    }
    return QDate::currentDate().year();
}

} // namespace

Santander::Santander(const QString &typeAccount)
    : Bank(QStringLiteral("Santander"), typeAccount) {}

Santander::Santander(const QString &typeAccount, const QString &filePath)
    : Bank(QStringLiteral("Santander"), typeAccount, filePath) {}

void Santander::readBankMovementsDebit(const QStringList &pagesText,
                                       QList<Transaction> &out)
{
    const int year = statementYear(pagesText);

    auto flush = [&](const QString &row) {
        if (row.isEmpty()) return;
        const auto m = rowRx().match(row);
        if (!m.hasMatch()) return;

        const QStringList dm = m.captured("date").split(QChar('/'));
        if (dm.size() != 2) return;
        const int day = dm[0].toInt();
        const int mon = dm[1].toInt();
        if (mon < 1 || mon > 12 || day < 1 || day > 31) return;

        const QString rawDesc = m.captured("desc").simplified();

        Transaction t;
        t.date        = QDateTime(QDate(year, mon, day), QTime(0, 0));
        t.amount      = qAbs(parseClpAmount(m.captured("amount")));
        t.account     = nameBank;
        t.category    = classifier.classify(rawDesc);
        t.description = cleanDescription(rawDesc);
        out.append(t);
    };

    QString current;
    for (const QString &page : pagesText) {
        const QStringList lines = page.split(QChar('\n'), Qt::KeepEmptyParts);
        for (const QString &raw : lines) {
            const QString line = raw.trimmed();
            if (line.isEmpty()) continue;

            if (rowStartRx().match(line).hasMatch()) {
                flush(current);
                current = line;
            } else if (!current.isEmpty()) {
                current.append(QChar(' ')).append(line);
            }
        }
    }
    flush(current);
}

void Santander::readBankMovementsCredit(const QStringList &pagesText,
                                        QList<Transaction> &out)
{
    Q_UNUSED(pagesText);
    Q_UNUSED(out);
    qWarning() << "[Santander/credit] parser not implemented yet — drop a "
                  "sample PDF in files/ and we can fill in the regexes.";
}
