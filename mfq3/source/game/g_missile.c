/*
 * $Id: g_missile.c,v 1.1 2001-11-15 21:35:14 thebjoern Exp $
*/

// Copyright (C) 1999-2000 Id Software, Inc.
//
#include "g_local.h"

#define	MISSILE_PRESTEP_TIME	50

/*
================
G_BounceMissile

================
*/
void G_BounceMissile( gentity_t *ent, trace_t *trace ) {
	vec3_t	velocity;
	float	dot;
	int		hitTime;

	// reflect the velocity on the trace plane
	hitTime = level.previousTime + ( level.time - level.previousTime ) * trace->fraction;
	BG_EvaluateTrajectoryDelta( &ent->s.pos, hitTime, velocity );
	dot = DotProduct( velocity, trace->plane.normal );
	VectorMA( velocity, -2*dot, trace->plane.normal, ent->s.pos.trDelta );

	if ( ent->s.eFlags & EF_BOUNCE_HALF ) {
		VectorScale( ent->s.pos.trDelta, 0.65, ent->s.pos.trDelta );
		// check for stop
		if ( trace->plane.normal[2] > 0.2 && VectorLength( ent->s.pos.trDelta ) < 40 ) {
			G_SetOrigin( ent, trace->endpos );
			return;
		}
	}

	VectorAdd( ent->r.currentOrigin, trace->plane.normal, ent->r.currentOrigin);
	VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
	ent->s.pos.trTime = level.time;
}


/*
================
G_ExplodeMissile

Explode a missile without an impact
================
*/
void G_ExplodeMissile( gentity_t *ent ) {
	vec3_t		dir;
	vec3_t		origin;

	BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );
	SnapVector( origin );
	G_SetOrigin( ent, origin );

	// we don't have a valid direction, so just point straight up
	dir[0] = dir[1] = 0;
	dir[2] = 1;

	ent->s.eType = ET_GENERAL;
	G_AddEvent( ent, EV_MISSILE_MISS, DirToByte( dir ) );

	ent->freeAfterEvent = qtrue;

	// splash damage
	if ( ent->splashDamage ) {
		if( G_RadiusDamage( ent->r.currentOrigin, ent->parent, ent->splashDamage, ent->splashRadius, ent
			, ent->splashMethodOfDeath ) ) {
			g_entities[ent->r.ownerNum].client->accuracy_hits++;
		}
	}

	trap_LinkEntity( ent );
}


/*
================
G_MissileImpact
================
*/
void G_MissileImpact( gentity_t *ent, trace_t *trace ) {
	gentity_t		*other;
	qboolean		hitClient = qfalse;

	other = &g_entities[trace->entityNum];

	// check for bounce
	if ( !other->takedamage &&
		( ent->s.eFlags & ( EF_BOUNCE | EF_BOUNCE_HALF ) ) ) {
		G_BounceMissile( ent, trace );
		return;
	}

	// impact damage
	if (other->takedamage) {
		// FIXME: wrong damage direction?
		if ( ent->damage ) {
			vec3_t	velocity;

			if( LogAccuracyHit( other, &g_entities[ent->r.ownerNum] ) ) {
				g_entities[ent->r.ownerNum].client->accuracy_hits++;
				hitClient = qtrue;
			}
			BG_EvaluateTrajectoryDelta( &ent->s.pos, level.time, velocity );
			if ( VectorLength( velocity ) == 0 ) {
				velocity[2] = 1;
			}
			G_Damage (other, ent, &g_entities[ent->r.ownerNum], velocity,
				ent->s.origin, ent->damage, 
				0, ent->methodOfDeath);
		}
	}

	// is it cheaper in bandwidth to just remove this ent and create a new
	// one, rather than changing the missile into the explosion?

	if ( other->takedamage && other->client ) {
		G_AddEvent( ent, EV_MISSILE_HIT, DirToByte( trace->plane.normal ) );
		ent->s.otherEntityNum = other->s.number;
	} else if( trace->surfaceFlags & SURF_METALSTEPS ) {
		G_AddEvent( ent, EV_MISSILE_MISS_METAL, DirToByte( trace->plane.normal ) );
	} else {
		G_AddEvent( ent, EV_MISSILE_MISS, DirToByte( trace->plane.normal ) );
	}

	ent->freeAfterEvent = qtrue;

	// change over to a normal entity right at the point of impact
	ent->s.eType = ET_GENERAL;

	SnapVectorTowards( trace->endpos, ent->s.pos.trBase );	// save net bandwidth

	G_SetOrigin( ent, trace->endpos );

	// splash damage (doesn't apply to person directly hit)
	if ( ent->splashDamage ) {
		if( G_RadiusDamage( trace->endpos, ent->parent, ent->splashDamage, ent->splashRadius, 
			other, ent->splashMethodOfDeath ) ) {
			if( !hitClient ) {
				g_entities[ent->r.ownerNum].client->accuracy_hits++;
			}
		}
	}

	trap_LinkEntity( ent );
}

/*
================
G_RunMissile
================
*/
void G_RunMissile( gentity_t *ent ) {
	vec3_t		origin;
	trace_t		tr;
	int			passent;

	// get current position
	BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );

	// if this missile bounced off an invulnerability sphere
	if ( ent->target_ent ) {
		passent = ent->target_ent->s.number;
	}
	else {
		// ignore interactions with the missile owner
		passent = ent->r.ownerNum;
	}
	// trace a line from the previous position to the current position
	trap_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, origin, passent, ent->clipmask );

	if ( tr.startsolid || tr.allsolid ) {
		// make sure the tr.entityNum is set to the entity we're stuck in
		trap_Trace( &tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, ent->r.currentOrigin, passent, ent->clipmask );
		tr.fraction = 0;
	}
	else {
		VectorCopy( tr.endpos, ent->r.currentOrigin );
	}

	trap_LinkEntity( ent );

	if ( tr.fraction != 1 ) {
		// never explode or bounce on sky
		if ( tr.surfaceFlags & SURF_NOIMPACT ) {
			G_FreeEntity( ent );
			return;
		}
		G_MissileImpact( ent, &tr );
		if ( ent->s.eType != ET_MISSILE &&
			 ent->s.eType != ET_BULLET ) {
			return;		// exploded
		}
	}

	// check think function after bouncing
	G_RunThink( ent );
}


//=============================================================================

/*
=================
fire_ffar MFQ3
=================
*/
void fire_ffar (gentity_t *self) {
	gentity_t	*bolt;
	vec3_t		dir, right, up;
	vec3_t		start;
	int			mult;

	AngleVectors( self->client->ps.vehicleAngles, dir, right, up );
	VectorCopy( self->s.pos.trBase, start );
	if( (availableVehicles[self->client->vehicle].id&CAT_ANY) & CAT_PLANE ) {
		self->left = (self->left ? qfalse : qtrue);
		mult = (self->left ? 1 : -1);
		VectorMA( start, availableVehicles[self->client->vehicle].mins[2], up, start );
		VectorMA( start, availableVehicles[self->client->vehicle].maxs[1]*mult/2, right, start );
	}
	SnapVector( start );

	bolt = G_Spawn();
	bolt->classname = "ffar";
	bolt->nextthink = level.time + 10000;
	bolt->think = G_ExplodeMissile;
	bolt->s.eType = ET_MISSILE;
	bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weaponIndex = self->client->ps.weaponIndex;
	bolt->r.ownerNum = self->s.number;
	bolt->parent = self;
	bolt->damage = bolt->splashDamage = availableWeapons[self->client->ps.weaponIndex].damage;
	bolt->splashRadius = availableWeapons[self->client->ps.weaponIndex].damageRadius;
	bolt->methodOfDeath = MOD_FFAR;
	bolt->splashMethodOfDeath = MOD_FFAR_SPLASH;
	bolt->clipmask = MASK_SHOT;
	bolt->target_ent = NULL;

	bolt->s.pos.trType = TR_LINEAR;
	bolt->s.pos.trTime = level.time;// - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
	VectorCopy( start, bolt->s.pos.trBase );
	VectorScale( dir, availableWeapons[self->client->ps.weaponIndex].muzzleVelocity, bolt->s.pos.trDelta );
	SnapVector( bolt->s.pos.trDelta );			// save net bandwidth
	VectorCopy (start, bolt->r.currentOrigin);
}

/*
=================
fire_ironbomb MFQ3
=================
*/
void fire_ironbomb (gentity_t *self) {
	gentity_t	*bolt;
	vec3_t		dir, right, up;
	vec3_t		start;
	int			mult;

	AngleVectors( self->client->ps.vehicleAngles, dir, right, up );
	VectorCopy( self->s.pos.trBase, start );
	if( (availableVehicles[self->client->vehicle].id&CAT_ANY) & CAT_PLANE ) {
		self->left = (self->left ? qfalse : qtrue);
		mult = (self->left ? 1 : -1);
		VectorMA( start, availableVehicles[self->client->vehicle].mins[2], up, start );
		VectorMA( start, availableVehicles[self->client->vehicle].maxs[1]*mult/2, right, start );
	}
	SnapVector( start );

	bolt = G_Spawn();
	bolt->classname = "ironbomb";
	bolt->nextthink = level.time + 10000;
	bolt->think = G_ExplodeMissile;
	bolt->s.eType = ET_MISSILE;
	bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weaponIndex = self->client->ps.weaponIndex;
	bolt->r.ownerNum = self->s.number;
	bolt->parent = self;
	bolt->damage = bolt->splashDamage = availableWeapons[self->client->ps.weaponIndex].damage;
	bolt->splashRadius = availableWeapons[self->client->ps.weaponIndex].damageRadius;
	bolt->methodOfDeath = MOD_IRONBOMB;
	bolt->splashMethodOfDeath = MOD_IRONBOMB_SPLASH;
	bolt->clipmask = MASK_SHOT;
	bolt->target_ent = NULL;

	bolt->s.pos.trType = TR_GRAVITY;
	bolt->s.pos.trTime = level.time;// - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
	VectorCopy( start, bolt->s.pos.trBase );
	VectorScale( dir, self->client->ps.speed/10, bolt->s.pos.trDelta );
	SnapVector( bolt->s.pos.trDelta );			// save net bandwidth
	VectorCopy (start, bolt->r.currentOrigin);
}


/*
=================
fire_autocannon MFQ3
=================
*/
void fire_autocannon (gentity_t *self) {
	gentity_t	*bolt;
	vec3_t		dir, forward, right, up, temp;
	vec3_t		start, offset;
	vec3_t		spreadangle;
	float		spreadX = availableWeapons[availableVehicles[self->client->vehicle].weapons[0]].spread; 
	float		spreadY = spreadX;

	// used for spread
	VectorCopy( self->client->ps.vehicleAngles, spreadangle );
	spreadX = ((rand() % (unsigned int)spreadX) - spreadX/2)/10;
	spreadY = ((rand() % (unsigned int)spreadY) - spreadY/2)/10;
	spreadangle[0] += spreadX;
	spreadangle[1] += spreadY;
//	G_Printf( "spread %.1f %.1f\n", spreadX, spreadY );

	VectorCopy( availableVehicles[self->client->vehicle].gunoffset, offset );
	VectorCopy( self->s.pos.trBase, start );

	if( (availableVehicles[self->client->vehicle].id&CAT_ANY) & CAT_GROUND ) {
		// use this to make it shoot where the player looks
//		AngleVectors( self->client->ps.viewangles, dir, 0, 0 );
//		VectorCopy( self->s.pos.trBase, start );
//		start[2] += availableVehicles[self->client->vehicle].maxs[2];
		// otherwise use this
		AngleVectors( spreadangle, forward, right, up );
		RotatePointAroundVector( temp, up, forward, ((float)self->client->ps.turretAngle)/10 );
		CrossProduct( up, temp, right );
		RotatePointAroundVector( dir, right, temp, ((float)self->client->ps.gunAngle)/10 );

	} else {
		// planes and helos for now just shoot along their direction of flight
		AngleVectors( spreadangle, dir, right, up );
	}

	if( availableVehicles[self->client->vehicle].caps & HC_DUALGUNS ) {
		self->bulletpos = ( self->bulletpos == 0 ? 1 : 0 );
		if( self->bulletpos ) {
			offset[1] *= -1;
		}
	}

	VectorMA( start, offset[0], dir, start );
	VectorMA( start, offset[1], right, start );
	VectorMA( start, offset[2], up, start );
	SnapVector( start );

	bolt = G_Spawn();
	bolt->classname = "bullet";
	bolt->nextthink = level.time + 4000;
	bolt->think = G_FreeEntity;
	bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weaponIndex = availableVehicles[self->client->vehicle].weapons[WP_MACHINEGUN];
	bolt->r.ownerNum = self->s.number;
	bolt->parent = self;
	bolt->damage = availableWeapons[availableVehicles[self->client->vehicle].weapons[WP_MACHINEGUN]].damage;
	bolt->methodOfDeath = MOD_AUTOCANNON;
	bolt->clipmask = MASK_SHOT;
	bolt->target_ent = NULL;
	bolt->s.eType = ET_BULLET;
	bolt->s.pos.trType = TR_LINEAR;
	bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
	//tracer
	self->tracerindex --;
	if( self->tracerindex < 0 ) self->tracerindex = 255;
	bolt->s.generic1 = self->tracerindex;
	VectorCopy( start, bolt->s.pos.trBase );
	VectorScale( dir, availableWeapons[availableVehicles[self->client->vehicle].weapons[WP_MACHINEGUN]].muzzleVelocity, bolt->s.pos.trDelta );
	SnapVector( bolt->s.pos.trDelta );			// save net bandwidth
	VectorCopy (start, bolt->r.currentOrigin);
}


/*
=================
fire_maingun MFQ3
=================
*/
void fire_maingun( gentity_t *self ) {
	gentity_t	*bolt;
	vec3_t		dir, forward, right, up, temp;
	vec3_t		start, offset;

	AngleVectors( self->client->ps.vehicleAngles, forward, right, up );
	RotatePointAroundVector( temp, up, forward, ((float)self->client->ps.turretAngle)/10 );
	CrossProduct( up, temp, right );
	RotatePointAroundVector( dir, right, temp, ((float)self->client->ps.gunAngle)/10 );

#pragma message("we NEED access to the guntag in order to properly place the gunshell!!!!!!!!!!!!!")
	VectorCopy( self->s.pos.trBase, start );
	VectorCopy( availableVehicles[self->client->vehicle].gunoffset, offset );
	VectorMA( start, offset[0], dir, start );
	VectorMA( start, offset[1], right, start );
	VectorMA( start, offset[2], up, start );
	SnapVector( start );

	bolt = G_Spawn();
	bolt->classname = "shell";
	bolt->nextthink = level.time + 15000;
	bolt->think = G_ExplodeMissile;
	bolt->s.eType = ET_MISSILE;
	bolt->r.svFlags = SVF_USE_CURRENT_ORIGIN;
	bolt->s.weaponIndex = self->client->ps.weaponIndex;
	bolt->r.ownerNum = self->s.number;
	bolt->parent = self;
	bolt->damage = bolt->splashDamage = availableWeapons[self->client->ps.weaponIndex].damage;
	bolt->splashRadius = availableWeapons[self->client->ps.weaponIndex].damageRadius;
	bolt->methodOfDeath = MOD_MAINGUN;
	bolt->splashMethodOfDeath = MOD_MAINGUN;
	bolt->clipmask = MASK_SHOT;
	bolt->target_ent = NULL;

	bolt->s.pos.trType = TR_GRAVITY;
	bolt->s.pos.trTime = level.time;// - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
	VectorCopy( start, bolt->s.pos.trBase );
	VectorScale( dir, availableWeapons[self->client->ps.weaponIndex].muzzleVelocity, bolt->s.pos.trDelta );
	SnapVector( bolt->s.pos.trDelta );			// save net bandwidth
	VectorCopy (start, bolt->r.currentOrigin);
}
