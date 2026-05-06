#include "bank.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QProcess>

#ifndef PDFREADER_PROJECT_ROOT
#define PDFREADER_PROJECT_ROOT ""
#endif

namespace {

// Resolve the python interpreter and extractor script to use.
// Override via env: PDFREADER_EXTRACTOR_PYTHON, PDFREADER_EXTRACTOR_SCRIPT.
// When PDFREADER_EXTRACTOR_BUNDLE is set, it points at a single PyInstaller
// binary (no separate interpreter / script).
struct ExtractorCmd {
    QString program;
    QStringList args;   // first arg appended later is the PDF path
    bool valid {false};
};

ExtractorCmd resolveExtractor()
{
    const auto env = QProcessEnvironment::systemEnvironment();

    if (env.contains(QStringLiteral("PDFREADER_EXTRACTOR_BUNDLE"))) {
        ExtractorCmd c;
        c.program = env.value(QStringLiteral("PDFREADER_EXTRACTOR_BUNDLE"));
        c.valid = QFileInfo::exists(c.program);
        return c;
    }

    const QStringList roots {
        env.value(QStringLiteral("PDFREADER_PROJECT_ROOT")),
        QCoreApplication::applicationDirPath(),
        QDir::currentPath(),
        QString::fromUtf8(PDFREADER_PROJECT_ROOT),
    };

    QString python = env.value(QStringLiteral("PDFREADER_EXTRACTOR_PYTHON"));
    QString script = env.value(QStringLiteral("PDFREADER_EXTRACTOR_SCRIPT"));

    for (const QString &root : roots) {
        if (root.isEmpty()) continue;
        if (python.isEmpty()) {
            const QString candidate = QDir(root).filePath(QStringLiteral(".venv/bin/python"));
            if (QFileInfo::exists(candidate)) python = candidate;
        }
        if (script.isEmpty()) {
            const QString candidate = QDir(root).filePath(QStringLiteral("tools/extract_pdf.py"));
            if (QFileInfo::exists(candidate)) script = candidate;
        }
        if (!python.isEmpty() && !script.isEmpty()) break;
    }

    if (python.isEmpty()) python = QStringLiteral("python3");

    ExtractorCmd c;
    c.program = python;
    c.args << script;
    c.valid = !script.isEmpty() && QFileInfo::exists(script);
    return c;
}

} // namespace

Bank::Bank(const QString &nameBank,
           const QString &typeAccount)
    : nameBank(nameBank)
    , typeAccount(typeAccount)
{}

Bank::Bank(const QString &nameBank,
           const QString &typeAccount,
           const QString &filePath)
    : nameBank(nameBank)
    , typeAccount(typeAccount)
    , filePath(filePath)
{}

void Bank::readBankMovements(const QString &filePath)
{
    this->filePath = filePath;

    const QStringList pages = extractPdfText(filePath);
    if (pages.isEmpty()) {
        qWarning() << "[Bank]" << nameBank
                   << "no text extracted from" << filePath;
        return;
    }

    transactions.clear();

    const QString kind = typeAccount.trimmed().toLower();
    if (kind == QStringLiteral("debit")) {
        readBankMovementsDebit(pages, transactions);
    } else if (kind == QStringLiteral("credit")) {
        readBankMovementsCredit(pages, transactions);
    } else {
        qWarning().noquote()
            << "[Bank]" << nameBank
            << "unknown typeAccount:" << typeAccount
            << "— expected 'debit' or 'credit'. Defaulting to debit.";
        readBankMovementsDebit(pages, transactions);
    }
}

void Bank::dumpTransactions() const
{
    qDebug().noquote() << QStringLiteral("[%1/%2] %3 transactions")
                              .arg(nameBank, typeAccount).arg(transactions.size());
    for (const Transaction &t : transactions) {
        qDebug().noquote() << QStringLiteral("  %1  %2  %3  [%4]")
            .arg(t.date.toString(QStringLiteral("yyyy-MM-dd")))
            .arg(t.amount, 12, 'f', 0)
            .arg(t.description.left(60), -60)
            .arg(t.category);
    }
}

void Bank::printBankFile() const
{
    printBankFile(filePath);
}

void Bank::printBankFile(const QString &filePath) const
{
    const QStringList pages = extractPdfText(filePath);
    for (int p = 0; p < pages.size(); ++p) {
        qDebug().noquote() << QStringLiteral("---- page %1 ----").arg(p + 1);
        const QStringList lines = pages.at(p).split(QChar('\n'), Qt::KeepEmptyParts);
        for (int l = 0; l < lines.size(); ++l) {
            qDebug().noquote()
                << QStringLiteral("[p%1:l%2] %3")
                       .arg(p + 1, 2, 10, QChar('0'))
                       .arg(l + 1, 3, 10, QChar('0'))
                       .arg(lines.at(l));
        }
    }
}

QStringList Bank::extractPdfText(const QString &filePath) const
{
    QStringList out;

    if (!QFileInfo::exists(filePath)) {
        qWarning() << "[Bank] file not found:" << filePath;
        return out;
    }

    const ExtractorCmd cmd = resolveExtractor();
    if (!cmd.valid) {
        qWarning() << "[Bank] extractor not found. Set PDFREADER_EXTRACTOR_SCRIPT "
                      "or PDFREADER_EXTRACTOR_BUNDLE, or place tools/extract_pdf.py "
                      "next to the executable.";
        return out;
    }

    QStringList args = cmd.args;
    args << filePath;

    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start(cmd.program, args);
    if (!proc.waitForStarted(5000)) {
        qWarning() << "[Bank] failed to start extractor:" << cmd.program
                   << proc.errorString();
        return out;
    }
    if (!proc.waitForFinished(60000)) {
        qWarning() << "[Bank] extractor timed out";
        proc.kill();
        proc.waitForFinished(2000);
        return out;
    }

    const QByteArray stdoutBytes = proc.readAllStandardOutput();
    const QByteArray stderrBytes = proc.readAllStandardError();
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        qWarning().noquote() << "[Bank] extractor failed (exit"
                             << proc.exitCode() << "):" << QString::fromUtf8(stderrBytes);
        return out;
    }

    QJsonParseError jerr;
    const QJsonDocument doc = QJsonDocument::fromJson(stdoutBytes, &jerr);
    if (jerr.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[Bank] invalid JSON from extractor:" << jerr.errorString();
        return out;
    }

    const QJsonObject root = doc.object();
    if (root.contains(QStringLiteral("error"))) {
        qWarning().noquote() << "[Bank] extractor error:"
                             << root.value(QStringLiteral("error")).toString();
        return out;
    }

    const QJsonArray pages = root.value(QStringLiteral("pages")).toArray();
    out.reserve(pages.size());
    for (const QJsonValue &v : pages)
        out.append(v.toString());

    return out;
}

int Bank::spanishMonth(const QString &name)
{
    static const QHash<QString, int> table {
        { QStringLiteral("ene"),  1 }, { QStringLiteral("feb"),  2 },
        { QStringLiteral("mar"),  3 }, { QStringLiteral("abr"),  4 },
        { QStringLiteral("may"),  5 }, { QStringLiteral("jun"),  6 },
        { QStringLiteral("jul"),  7 }, { QStringLiteral("ago"),  8 },
        { QStringLiteral("sep"),  9 }, { QStringLiteral("oct"), 10 },
        { QStringLiteral("nov"), 11 }, { QStringLiteral("dic"), 12 },
    };
    return table.value(name.left(3).toLower(), 0);
}

QString Bank::cleanDescription(QString desc)
{
    // RUTs: "Rut 19.589.611-9", or bare "19.589.611-9".
    static const QRegularExpression rutRx(
        QStringLiteral(R"(\b(?:Rut\s+)?\d{1,2}\.\d{3}\.\d{3}-[\dkK]\b)"),
        QRegularExpression::CaseInsensitiveOption);

    // Time + "hrs": "a las 19:15:34 hrs.", "a las 11:34", "a las21:33 hrs."
    static const QRegularExpression timeRx(
        QStringLiteral(R"(\ba\s*las\s*\d{1,2}:\d{2}(?::\d{2})?\s*hrs?\.?)"),
        QRegularExpression::CaseInsensitiveOption);

    // Dates: "el 02/04/2026", "el17/04/2026", "el 2026-04-04", "el2026-04-04".
    static const QRegularExpression dateRx(
        QStringLiteral(R"(\bel\s*(?:\d{1,2}/\d{1,2}/\d{2,4}|\d{4}-\d{2}-\d{2})\b)"),
        QRegularExpression::CaseInsensitiveOption);

    // Bare "DD/MM/YYYY" / "YYYY-MM-DD" left over.
    static const QRegularExpression bareDateRx(
        QStringLiteral(R"(\b(?:\d{1,2}/\d{1,2}/\d{2,4}|\d{4}-\d{2}-\d{2})\b)"));

    // Bare time, with or without "hrs" suffix: "10:44:51", "10:44:51 hrs.".
    static const QRegularExpression bareTimeRx(
        QStringLiteral(R"(\b\d{1,2}:\d{2}(?::\d{2})?(?:\s*hrs?\.?)?)"),
        QRegularExpression::CaseInsensitiveOption);

    // "monto $ 25.484,00" / ", monto 25100." / ", monto $" (value already stripped).
    static const QRegularExpression montoRx(
        QStringLiteral(R"(,?\s*monto\s*\$?\s*[\d\.,]*\.?)"),
        QRegularExpression::CaseInsensitiveOption);

    // Stray operation numbers: "Nro. Oper. 2026042078931435", "boleta N 001957182018".
    static const QRegularExpression opNrRx(
        QStringLiteral(R"(\b(?:Nro\.?\s*Oper\.?|boleta\s*N\.?)\s*\d+)"),
        QRegularExpression::CaseInsensitiveOption);

    // Bare numeric IDs of 10+ digits: Santander packs RUT/account/operation
    // identifiers as e.g. "011111111111". Shorter numbers (amounts, postal
    // codes) are left alone.
    static const QRegularExpression longIdRx(
        QStringLiteral(R"(\b\d{10,}\b)"));

    // Bank-specific operation prefixes / branch names that show up inside
    // descriptions but carry no semantic value once classified.
    static const QRegularExpression bankNoiseRx(
        QStringLiteral(R"(\b(?:O\.\s*Gerencia|Agustinas)\b)"),
        QRegularExpression::CaseInsensitiveOption);

    desc = desc.simplified();
    desc.remove(timeRx);
    desc.remove(dateRx);
    desc.remove(bareTimeRx);
    desc.remove(bareDateRx);
    desc.remove(rutRx);
    desc.remove(opNrRx);
    desc.remove(longIdRx);
    desc.remove(bankNoiseRx);
    desc.remove(montoRx);   // last: dangling "monto $" left by stripped value

    // Drop dangling connectors that lost their target.
    static const QRegularExpression connectorRx(
        QStringLiteral(R"(,?\s*\b(?:a\s*las|el|hrs?\.?|via\s+internet)\b)"),
        QRegularExpression::CaseInsensitiveOption);
    desc.remove(connectorRx);

    desc.replace(QRegularExpression(QStringLiteral(R"(\s*,\s*,+\s*)")),
                 QStringLiteral(", "));
    desc.replace(QRegularExpression(QStringLiteral(R"(\s+)")),
                 QStringLiteral(" "));
    desc.remove(QRegularExpression(QStringLiteral(R"([\s,\.;:]+$)")));

    return desc.trimmed();
}

QStringList Bank::unwrapRows(const QString &pageText,
                             const QRegularExpression &rowStartRx)
{
    QStringList rows;
    const QStringList lines = pageText.split(QChar('\n'));
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) continue;

        if (rowStartRx.match(line).hasMatch())
            rows.append(line);
        else if (!rows.isEmpty())
            rows.last().append(QChar(' ')).append(line);
        // else: pre-table noise, drop.
    }
    return rows;
}

double Bank::parseClpAmount(const QString &raw)
{
    static const QRegularExpression keepRx(QStringLiteral("[^0-9,.\\-]"));
    QString s = raw;
    s.remove(keepRx);

    if (s.contains(QChar(','))) {
        s.remove(QChar('.'));        // dots are thousands separators
        s.replace(QChar(','), QChar('.')); // comma -> decimal point
    } else {
        s.remove(QChar('.'));        // CLP integer with thousands separators
    }
    bool ok = false;
    const double v = s.toDouble(&ok);
    return ok ? v : 0.0;
}
