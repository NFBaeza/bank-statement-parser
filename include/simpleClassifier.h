#ifndef SIMPLECLASSIFIER_H
#define SIMPLECLASSIFIER_H

#include <QString>
#include <QList>
#include <QRegularExpression>

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

#endif // SIMPLECLASSIFIER_H
