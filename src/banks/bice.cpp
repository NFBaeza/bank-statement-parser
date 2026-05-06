#include "banks/bice.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QRegularExpression>
#include <QTime>

namespace {

// BICE rows always start with "<day> <mon> <year> Cargos|Abonos".
// Anything else is a wrap of the previous row.
const QRegularExpression &rowStartRx()
{
    static const QRegularExpression rx(
        QStringLiteral(R"(^\s*\d{1,2}\s+[A-Za-zñÑ]{3,}\.?\s+\d{4}\s+(?:Cargos|Abonos)\b)"),
        QRegularExpression::CaseInsensitiveOption);
    return rx;
}

// Captures the first $-amount on the (un-wrapped) row; BICE prints the
// posted amount right-aligned at the end of the first physical line.
const QRegularExpression &rowRx()
{
    static const QRegularExpression rx(
        QStringLiteral(
            R"(^(?<day>\d{1,2})\s+(?<mon>[A-Za-zñÑ]{3,})\.?\s+(?<year>\d{4})\s+)"
            R"((?<section>Cargos|Abonos)\s+)"
            R"((?<op>\S+)\s+)"
            R"((?<desc>.*?)\s+\$\s*(?<amount>-?[\d\.,]+)\b)"
        ),
        QRegularExpression::CaseInsensitiveOption);
    return rx;
}

} // namespace

BICE::BICE(const QString &typeAccount)
    : Bank(QStringLiteral("BICE"), typeAccount) {}

BICE::BICE(const QString &typeAccount, const QString &filePath)
    : Bank(QStringLiteral("BICE"), typeAccount, filePath) {}

void BICE::readBankMovementsDebit(const QStringList &pagesText,
                                  QList<Transaction> &out)
{
    auto flush = [&](const QString &row) {
        if (row.isEmpty()) return;
        const auto m = rowRx().match(row);
        if (!m.hasMatch()) return;

        const int day  = m.captured("day").toInt();
        const int mon  = spanishMonth(m.captured("mon"));
        const int year = m.captured("year").toInt();
        if (mon == 0) return;

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
    bool reachedEnd = false;

    for (int p = 0; p < pagesText.size() && !reachedEnd; ++p) {
        const QStringList lines = pagesText.at(p).split(QChar('\n'),
                                                        Qt::KeepEmptyParts);
        for (const QString &raw : lines) {
            const QString line = raw.trimmed();

            if (line.startsWith(QStringLiteral("Página ")))
                break;
            if (line.contains(QStringLiteral("Saldos diarios"))) {
                reachedEnd = true;
                break;
            }
            if (line.isEmpty()) continue;

            if (rowStartRx().match(line).hasMatch()) {
                flush(current);
                current = line;
            } else if (!current.isEmpty()) {
                current.append(QChar(' ')).append(line);
            }
            // else: pre-table noise (page header, summary block) — drop.
        }
    }
    flush(current);
}

void BICE::readBankMovementsCredit(const QStringList &pagesText,
                                   QList<Transaction> &out)
{
    Q_UNUSED(pagesText);
    Q_UNUSED(out);
    qWarning() << "[BICE/credit] parser not implemented yet — drop a sample "
                  "PDF in files/ and we can fill in the regexes.";
}
