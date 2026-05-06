#include "banks/bice.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QRegularExpression>
#include <QTime>

namespace {

// BICE-specific: rows start with "<day> <mon> <year> Cargos|Abonos".
// Anything else is a wrap of the previous row.
const QRegularExpression &rowStartRx()
{
    static const QRegularExpression rx(
        QStringLiteral(R"(^\s*\d{1,2}\s+[A-Za-zñÑ]{3,}\.?\s+\d{4}\s+(?:Cargos|Abonos)\b)"),
        QRegularExpression::CaseInsensitiveOption);
    return rx;
}

const QRegularExpression &rowRx()
{
    // Captures the first $-amount on the (un-wrapped) row; BICE prints the
    // posted amount right-aligned at the end of the first physical line.
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

double BICE::parseClpAmount(const QString &raw)
{
    QString s = raw;
    s.remove(QRegularExpression(QStringLiteral("[^0-9,.\\-]")));

    const bool hasComma = s.contains(',');
    if (hasComma) {
        s.remove('.');
        s.replace(',', '.');
    } else {
        s.remove('.');
    }
    bool ok = false;
    const double v = s.toDouble(&ok);
    return ok ? v : 0.0;
}

void BICE::readBankMovementsDebit(const QStringList &pagesText,
                                  QList<Transaction> &out)
{
    // Helper: parse one fully-unwrapped row and append a Transaction.
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
        t.account     = QStringLiteral("debit");
        t.category    = classifier.classify(rawDesc);
        t.description = cleanDescription(rawDesc);
        out.append(t);
    };

    QString current;
    bool reachedEnd = false;

    for (int p = 0; p < pagesText.size() && !reachedEnd; ++p) {
        const QStringList lines = pagesText.at(p).split(QChar('\n'),
                                                        Qt::KeepEmptyParts);
        int l = (p == 0) ? 15 : 0;
        for (; l < lines.size(); ++l) {
            const QString line = lines.at(l).trimmed();

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
        }
    }
    flush(current);

    for (const Transaction &t : out) {
        qDebug().noquote() << QStringLiteral("  %1  %2  %3  [%4]")
            .arg(t.date.toString(QStringLiteral("yyyy-MM-dd")))
            .arg(t.amount, 12, 'f', 0)
            .arg(t.description.left(60), -60)
            .arg(t.category);
    }

    qDebug() << "[BICE/debit] parsed" << out.size() << "transactions";
}

void BICE::readBankMovementsCredit(const QStringList &pagesText,
                                   QList<Transaction> &out)
{
    static const QRegularExpression rx(
        QStringLiteral(
            "(?<date>\\d{1,2}/\\d{1,2}(?:/\\d{2,4})?)\\s+"
            "(?<desc>.+?)\\s+"
            "(?<amount>-?\\$?\\s*[\\d\\.]+(?:,\\d+)?)\\s*$"
        ),
        QRegularExpression::MultilineOption);

    for (const QString &page : pagesText) {
        auto it = rx.globalMatch(page);
        while (it.hasNext()) {
            const auto m = it.next();
            Transaction t;
            t.date        = castQDateTime(m.captured("date"));
            t.description = m.captured("desc").simplified();
            t.amount      = parseClpAmount(m.captured("amount"));
            t.account     = QStringLiteral("credit");
            t.category    = classifier.classify(t.description);
            out.append(t);
        }
    }
    qDebug() << "[BICE/credit] parsed" << out.size() << "transactions";
}
