#ifndef PDFPARSER_SIMPLECLASSIFIER_H
#define PDFPARSER_SIMPLECLASSIFIER_H

#include <QString>
#include <QList>
#include <QRegularExpression>

namespace pdfparser {

class SimpleClassifier
{
public:
    SimpleClassifier();

    QString classify(const QString &description) const;

private:
    struct Rule {
        QRegularExpression pattern;
        QString            category;
    };

    void initRules();

    QList<Rule> m_rules;
};

} // namespace pdfparser

#endif // PDFPARSER_SIMPLECLASSIFIER_H
