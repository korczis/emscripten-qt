#include "attractoraffector.h"
#include <cmath>

AttractorAffector::AttractorAffector(QObject *parent) :
    ParticleAffector(parent), m_strength(0.0), m_x(0), m_y(0)
{
}

bool AttractorAffector::affect(ParticleData *d, qreal dt)
{
    if(m_strength == 0.0)
        return false;
    qreal dx = m_x - d->curX();
    qreal dy = m_y - d->curY();
    qreal r = sqrt((dx*dx) + (dy*dy));
    qreal theta = atan2(dy,dx);
    qreal ds = (m_strength / r) * dt;
    dx = ds * cos(theta);
    dy = ds * sin(theta);
    d->setInstantaneousSX(d->pv.sx + dx);
    d->setInstantaneousSY(d->pv.sy + dy);
    return true;
}