#include "tower.h"
#include "rolemanager.h"
#include "server.h"
#include "../global/guid.h"
#include "../global/octets.h"
#include "../global/kv.h"
#include "../global/logClient.h"
#include "../global/rolepacket.h"
#include "../global/battlepool.h"
#include "../global/luabattle.h"
#include "fightmode.h"
#include "bigworldcounter.h"

void GameTower::Reset()
{
	_parent = NULL;
}

I32 GameTower::Load(GameRole* parent)
{
	Reset();

	if ( !parent )
		return ERR_SERVER_DATA;

	_parent = parent;

	return 0;
}

void GameTower::ActivityCheck()
{
	I32 lv = _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT);
	if(lv > _parent->GetCounter()->GetValue(GCID_WARRIOR_SEVEN_IDX1, WARRIOR_DAY5_TOWER_LEVEL))
	{
		_parent->GetCounter()->SetValueWithActivity(GCID_WARRIOR_SEVEN_IDX1, WARRIOR_DAY5_TOWER_LEVEL, lv);
	}
}

I32 GameTower::getCounterByType(I32 type, I32& currentLevel)
{
	if (type == 0)
		currentLevel = _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT);
	else if (type == 1)
		currentLevel = _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_1);
	else if (type == 2)
		currentLevel = _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_2);
	else if (type == 3)
		currentLevel = _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_3);
	else
		return ERR_CLIENT_DATA;
	return 0;
}

I32 GameTower::CheckChallengeTimes(I32 type)
{
	if (type == 0)
		return 0;
	I32 usedTimes = 0;
	if (type == 1)
		usedTimes = _parent->GetCounter()->GetValue(GCID_UP2400, GUP2400_TOWER_WIN1);
	if (type == 2)
		usedTimes = _parent->GetCounter()->GetValue(GCID_UP2400, GUP2400_TOWER_WIN2);
	if (type == 3)
		usedTimes = _parent->GetCounter()->GetValue(GCID_UP2400, GUP2400_TOWER_WIN3);
	const TempGroupTConfigCfg* groupCfg = TEMPMGR->GetOneGroupTConfigCfg(1);
	return groupCfg->winTimes <= usedTimes;
}

I32 GameTower::CheckParamChallenge(I32 towerLevel, I32 type)
{
	I32 ret = CheckParam(towerLevel, type);
	if (ret)
		return ret;
	
	// 检查目标层数是否存在
	I32 currentLevel = 0;
	ret = getCounterByType(type, currentLevel);
	if (ret)
		return ret;
	if ( towerLevel != currentLevel + 1 )
		return ERR_CLIENT_DATA;
	
	// 检查配置
	const TempTowerCfg* pCfg = TEMPMGR->GetOneTowerCfg(towerLevel);
	if ( !pCfg )
		return ERR_SERVER_DATA;
	
	// 达到最高层数了
	if (pCfg->info[type]._battleId == 0)
		return ERR_GODANIMAL_CACERN_MAXFLOOR;

	return ret;
}

I32 GameTower::CheckParam(I32 towerLevel, I32 type)
{
	// 功能开放
	I32 ret = _parent->CheckFunctionUnlock(FUT_ENEMY_COME);
	if ( ret )
		return ret;

	// 检查配置
	const TempTowerCfg* pCfg = TEMPMGR->GetOneTowerCfg(towerLevel);
	if ( !pCfg )
		return ERR_SERVER_DATA;
	const TempGroupTConfigCfg* groupCfg = TEMPMGR->GetOneGroupTConfigCfg(1);
	if ( !groupCfg || type < 0 || type > 3)
		return ERR_SERVER_DATA;
	
	// 检查开放时间
	if (type != 0)
	{
		I32 ret = TIMEMGR->CheckTime1(groupCfg->timeLimit[type-1]);
		if ( ret )
			return ret;
	}

	return 0;
}

I32 GameTower::OnPreChallenge(C2S_TowerPreChallengeData const& data, S2C_TowerPreChallengeReData* sdata)
{
	I32 ret = CheckParamChallenge(data.towerLevel, data.type);
	if (ret)
		return ret;

	// 检查获胜次数
	if (CheckChallengeTimes(data.type))
		return ERR_EXERCISE_CHALLENGETIME_NOT_ENOUGH;

	const TempTowerCfg* pCfg = TEMPMGR->GetOneTowerCfg(data.towerLevel);
	if (!pCfg)
		return ERR_SERVER_DATA;

	if (pCfg->info[data.type]._reward == 0)
		return ERR_SERVER_DATA;
	ret = _parent->CheckRewardId(pCfg->info[data.type]._reward);
	if ( ret )
		return ret;


	if ( sdata )
	{
		switch (data.type)
		{
			case TOWER_TYPE_tower:
				ret = _parent->GetSociety()->GetMercenary()->OnPreBattle(BATTLE_TYPE_fower, data.heros);
				break;
			case TOWER_TYPE_tower1:
				ret = _parent->GetSociety()->GetMercenary()->OnPreBattle(BATTLE_TYPE_fower1, data.heros);
				break;
			case TOWER_TYPE_tower2:
				ret = _parent->GetSociety()->GetMercenary()->OnPreBattle(BATTLE_TYPE_fower2, data.heros);
				break;
			case TOWER_TYPE_tower3:
				ret = _parent->GetSociety()->GetMercenary()->OnPreBattle(BATTLE_TYPE_fower3, data.heros);
				break;
			default:
				ret = ERR_CLIENT_DATA;
				break;
		}
		if(ret)
			return ret;

		ret = _parent->GetTeam()->FormatFighterInfo(data.heros, sdata->fighters.self);
		if (ret)
			return ret;
		// 检查上阵英雄类型是否匹配种族塔
		I32 count = 0;
		for (auto it = sdata->fighters.self.heros.begin(); it != sdata->fighters.self.heros.end(); ++it)
		{
			const TempHeroWuqiCfg *pHeroCfg = TEMPMGR->GetOneHeroWuqiCfg( it->hero.sid );
			if(pHeroCfg==NULL)
				return ERR_CLIENT_DATA;
			if (data.type > 0)
			{
				// 非神魔种族塔
				if (data.type < 4)
				{
					if (pHeroCfg->group == data.type)
					{
						count++;
					}
				}
				else
				{
					if (pHeroCfg->group >= 4)
					{
						count++;
					}
				}
			}
		}

		// 有数量限制的时候检查数量
		if (pCfg->info[data.type].BattleLimit > 0 && count < pCfg->info[data.type].BattleLimit)
		{
			return ERR_ZOMBIE_HERO;
		}
		
		if(sdata->fighters.self.power < pCfg->info[data.type].MinPower)
		{
			return ERR_FIGHT_TOOLOW;
		}

		ret = BATTLEPOOL->Format(pCfg->info[data.type]._battleId, sdata->fighters.enemy);
		if ( ret )
			return ret;

		//通关阵容新增-------------------------
		
		if (m_through_chapter_info.oneInfo.items.size() == 0)
		{
			RDThroughChapterItem tmp;
			m_through_chapter_info.oneInfo.items.push_back(tmp);
			m_power.push_back(sdata->fighters.self.power);
		}
		else
			m_power[0] = sdata->fighters.self.power;

		m_through_chapter_info.oneInfo.items[0].selfbattleArray.resize(sdata->fighters.self.heros.size());
		m_through_chapter_info.oneInfo.items[0].enemybattleArray.resize(sdata->fighters.enemy.heros.size());

		I32 index = 0;
		for (auto it = sdata->fighters.self.heros.begin(); it != sdata->fighters.self.heros.end(); ++it, ++index)
		{
			m_through_chapter_info.oneInfo.items[0].selfbattleArray[index] = it->hero;
		}

		index = 0;
		for (auto it = sdata->fighters.enemy.heros.begin(); it != sdata->fighters.enemy.heros.end(); ++it, ++index)
		{
			m_through_chapter_info.oneInfo.items[0].enemybattleArray[index] = it->hero;
		}
		//-------------------------------------

		_parent->GetCheckSum()->BeginFight(sdata->fighters);
        _parent->GetCounter()->IncValue(GCID_UP2400, GUP2400_TOWER_ALL_CHALLENGE);
	}
	
	_parent->SetDirty(GDM_BASIC_DIRTY);

	return 0;
}

//-----------------------------------------pre
I32 GameTower::OnTowerPreChallenge(const C2S_TowerPreChallengeData& data, S2C_TowerPreChallengeReData& sdata)
{
	return OnPreChallenge(data, &sdata);
}

void GameTower::setTowerLevel_debug(I32 lev) {

	const TempTowerCfg* pCfg = TEMPMGR->GetOneTowerCfg(lev);
	if ( !pCfg )
		return;
	bool flag = false;
	do {
		_parent->GetCounter()->SetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT, lev);
		_parent->GetCounter()->SetValue(GCID_NORMAL, GNORMAL_TOWER_MAX, lev);
	
		flag = true;
		
		if (pCfg->info[1]._reward == 0)
			break;
		_parent->GetCounter()->SetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_1, lev);
		
		if (pCfg->info[2]._reward == 0)
			return;
		_parent->GetCounter()->SetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_2, lev);
		
		if (pCfg->info[3]._reward == 0)
			return;
		_parent->GetCounter()->SetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_3, lev);
		
	} while (false);

	if (flag)
		_parent->SetDirty(GDM_RANK_DIRTY|GDM_CACHE_DIRTY);
	return;
}

I32 GameTower::OnRoleTowerPreChallenge(const C2S_TowerPreChallengeData& data)
{
	S2C_TowerPreChallengeReData sdata;
	sdata.roleId = data.roleId;
	sdata.towerLevel = data.towerLevel;
	sdata.ret = OnTowerPreChallenge(data, sdata);
	
	CPacket_Safe spacket(MSG_S2C_TowerPreChallengeRe);
	sdata.Encode(spacket);

	if (sdata.ret)
	{
		_parent->SendToGateWay(spacket);
	}
	else
	{
		I32 ret = 0;
		const char *jsonReplay = NULL;
		CReadPacket packet;
		CPacket_Safe spacket1(MSG_S2C_TowerPreChallengeRe);
		sdata.Encode(spacket1);

		const TempTowerCfg* pCfg = TEMPMGR->GetOneTowerCfg(data.towerLevel);
		if ( !pCfg )
			return ERR_SERVER_DATA;

		if (LuaBattle::GetInstance()->Battle(spacket1, packet, &jsonReplay))
		{
			// 服务器计算战斗失败, 发给客户端计算
			_parent->SendToGateWay(spacket);
		}
		else
		{
			_parent->SendBattleReplay(jsonReplay, MSG_S2C_TowerPostChallengeRe);


			C2S_TowerPostChallengeData dataLuaBattle;
			dataLuaBattle.Decode(packet);
			ret = OnRoleTowerPostChallenge(dataLuaBattle);
			XLOG_TRACE(ret, &dataLuaBattle);
		}
	}

	return sdata.ret;
}

IMPLEMENT_SVRFUN(MSG_C2S_TowerPreChallenge)
{
	C2S_TowerPreChallengeData data;
	data.Decode(packet);

	I32 ret = ERR_INVALID;

	GameRole *role = GRManager::GetInstance()->GetGameRoleByRid(data.roleId);
	if ( role )
		ret = role->GetTower()->OnRoleTowerPreChallenge(data);
	XLOG_TRACE(ret, &data);
}

//-----------------------------------------post
I32 GameTower::OnTowerPostChallenge(const C2S_TowerPostChallengeData& data, S2C_TowerPostChallengeReData& sdata)
{
	// 前面检查过对应配置了
	const TempTowerCfg* pCfg = TEMPMGR->GetOneTowerCfg(data.towerLevel);

	bool incLevel = false;

	// 检查获胜次数
	if (CheckChallengeTimes(data.type))
		return ERR_EXERCISE_CHALLENGETIME_NOT_ENOUGH;

	if ( data.result.skipped && data.result.rst )
	{
		incLevel = true;
	}
	else if ( data.result.rst )
	{
		I32 ret = _parent->GetCheckSum()->EndFight(data.result);
		if ( ret ) return ret;

		incLevel = (data.result.rst == 1);
	}

	if ( incLevel )
	{
		switch (data.type)
		{
			case TOWER_TYPE_tower:
				_parent->GetSociety()->GetMercenary()->OnPostBattle(BATTLE_TYPE_fower, data.result.selfInfo.heros);
				break;
			case TOWER_TYPE_tower1:
				_parent->GetSociety()->GetMercenary()->OnPostBattle(BATTLE_TYPE_fower1, data.result.selfInfo.heros);
				break;
			case TOWER_TYPE_tower2:
				_parent->GetSociety()->GetMercenary()->OnPostBattle(BATTLE_TYPE_fower2, data.result.selfInfo.heros);
				break;
			case TOWER_TYPE_tower3:
				_parent->GetSociety()->GetMercenary()->OnPostBattle(BATTLE_TYPE_fower3, data.result.selfInfo.heros);
				break;
			default:
				break;
		}

		I64 srchigh = 0;
		// 处理计数器
		if (0 == data.type)
		{
			_parent->GetCounter()->IncValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT);
			_parent->GetCounter()->IncValue(GCID_UPMONDAY, GUPMONDAY_Towe_Clear_Times);
			//_parent->GetCounter()->IncValue(GCID_UP2400, GUP2400_TOWER_ALL_CHALLENGE);
			I32 maxLevel = _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_MAX);
			I32 currentLevel = _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT);
			if ( currentLevel > maxLevel )
			{
				_parent->GetCounter()->SetValue(GCID_NORMAL, GNORMAL_TOWER_MAX, currentLevel);
				_parent->GetPushCharge()->TryTrigger(PushChargeType_Tower, currentLevel);
				_parent->GetCounter()->SetValueWithActivity(GCID_WARRIOR_SEVEN_IDX1, WARRIOR_DAY5_TOWER_LEVEL, currentLevel);
			}
			srchigh = RDMS_EN_TYPE(RDMS_TOWER);
			
			// 更新跨服计数器,提交世界成就
			GameBigworldCounter::GetInstance()->commitTower(_parent, currentLevel);
		}
		else if (1 == data.type)
		{
			_parent->GetCounter()->IncValue(GCID_UP2400, GUP2400_TOWER_WIN1);
			_parent->GetCounter()->IncValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_1);
			_parent->GetPushCharge()->TryTrigger(PushChargeType_Tower_LC, _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_1));
			srchigh = RDMS_EN_TYPE(RDMS_TOWER_1);
			
			I32 currentLevel = _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_1);
			// 更新跨服计数器,提交世界成就
			GameBigworldCounter::GetInstance()->commitTowerGROUP(_parent, currentLevel, data.type);
		}
		else if (2 == data.type)
		{
			_parent->GetCounter()->IncValue(GCID_UP2400, GUP2400_TOWER_WIN2);
			_parent->GetCounter()->IncValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_2);
			_parent->GetPushCharge()->TryTrigger(PushChargeType_Tower_QW, _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_2));
			srchigh = RDMS_EN_TYPE(RDMS_TOWER_2);
			
			I32 currentLevel = _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_2);
			// 更新跨服计数器,提交世界成就
			GameBigworldCounter::GetInstance()->commitTowerGROUP(_parent, currentLevel, data.type);
		}
		else if (3 == data.type)
		{
			_parent->GetCounter()->IncValue(GCID_UP2400, GUP2400_TOWER_WIN3);
			_parent->GetCounter()->IncValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_3);
			_parent->GetPushCharge()->TryTrigger(PushChargeType_Tower_DY, _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_3));
			srchigh = RDMS_EN_TYPE(RDMS_TOWER_3);
			
			I32 currentLevel = _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_3);
			// 更新跨服计数器,提交世界成就
			GameBigworldCounter::GetInstance()->commitTowerGROUP(_parent, currentLevel, data.type);
		}

		//热度榜上报--------------------------------
		{
			std::vector<I32> tmpv;
			for (auto it = data.result.selfInfo.heros.begin(); it != data.result.selfInfo.heros.end(); ++it)
			{
				if (it->id > 0)
				{
					tmpv.push_back(it->id);
				}
			}
			switch (data.type)
			{
				case TOWER_TYPE_tower:
					_parent->ReportHeatList(HEATLIST_SUBTYPE_TOWER, HEATLIST_TOWER_1, 0, tmpv);
					break;
				case TOWER_TYPE_tower1:
					_parent->ReportHeatList(HEATLIST_SUBTYPE_TOWER, HEATLIST_TOWER_2, 0, tmpv);
					break;
				case TOWER_TYPE_tower2:
					_parent->ReportHeatList(HEATLIST_SUBTYPE_TOWER, HEATLIST_TOWER_3, 0, tmpv);
					break;
				case TOWER_TYPE_tower3:
					_parent->ReportHeatList(HEATLIST_SUBTYPE_TOWER, HEATLIST_TOWER_4, 0, tmpv);
					break;
				default:
					break;
			}
		}
		//---------------------------------------------

		//通关阵容新增----------------------------

		m_through_chapter_info.oneInfo.items[0].selfbattleStatistical = data.result.selfInfo.heros;
		m_through_chapter_info.oneInfo.items[0].enemybattleStatistical = data.result.enemyInfo.heros;

		m_through_chapter_info.BWmsgHeader.roleId = sdata.roleId;
		m_through_chapter_info.BWmsgHeader.gamesid = GRManager::GetInstance()->GetLocalID();
		m_through_chapter_info.BWmsgHeader.tablesid = BIGWORLD_ACTIVITY_comm;
		m_through_chapter_info.roleId = sdata.roleId;

		if (0 == data.type)
		{
			m_through_chapter_info.typeId = 2;
			m_through_chapter_info.configId = _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT);
		}
		else if (1 == data.type)
		{
			m_through_chapter_info.typeId = 3;
			m_through_chapter_info.configId = _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_1);
		}
		else if (2 == data.type)
		{
			m_through_chapter_info.typeId = 4;
			m_through_chapter_info.configId = _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_2);
		}
		else if (3 == data.type)
		{
			m_through_chapter_info.typeId = 5;
			m_through_chapter_info.configId = _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_3);
		}


		m_through_chapter_info.oneInfo.passtime = Timer::GetTime();
		m_through_chapter_info.oneInfo.passlevel = _parent->GetBasic()->GetLevel();
		for (auto it = m_power.begin(); it != m_power.end(); ++it)
		{
			m_through_chapter_info.oneInfo.passpower += *it;
		}

		CPacket_Safe tmpspacket(G2BW_ThroughChapterNotify);
		m_through_chapter_info.Encode(tmpspacket);
		_parent->SendToGateWay(tmpspacket);



		m_power.clear();
		m_through_chapter_info.Clear();

		//----------------------------------------

		_parent->ProcessRewardId(pCfg->info[data.type]._reward, sdata.reward, srchigh | RDMS_EN_PARAM(data.towerLevel) );
		_parent->SetDirty(GDM_RANK_DIRTY|GDM_CACHE_DIRTY);

		// @note:活动计数器,从战斗胜利次数修改为战斗次数
		_parent->GetCounter()->IncValueWithActivity(GCID_WARRIOR_SEVEN_IDX1, WARRIOR_DAY3_TOWER_WINS_TIMES);
	}
	
	_parent->GetTeam()->SetTeam(TEAM_TYPE_TOWER, data.result.selfInfo.heros);

	_parent->SetDirty(GDM_BASIC_DIRTY);

	LOGCLIENT->Send2LogServer(elog_TowerChallenge, &data.roleId, data.towerLevel, data.result.skipped, data.result.rst );

	return 0;
}


// -- 深渊速通
I32 GameTower::OnTowerFastChallenge(const C2S_TowerFastChallengeData& data, S2C_TowerFastChallengeReData& sdata)
{
	// 前面检查过对应配置了
	const TempTowerCfg* pCfg = TEMPMGR->GetOneTowerCfg(data.towerLevel);

	// 检查获胜次数
	if (CheckChallengeTimes(data.type))	
		return ERR_EXERCISE_CHALLENGETIME_NOT_ENOUGH;

	I64 srchigh = 0;
	// 处理计数器
	if (0 == data.type)
	{
		_parent->GetCounter()->IncValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT);
		_parent->GetCounter()->IncValue(GCID_UPMONDAY, GUPMONDAY_Towe_Clear_Times);
		//_parent->GetCounter()->IncValue(GCID_UP2400, GUP2400_TOWER_ALL_CHALLENGE);
		I32 maxLevel = _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_MAX);
		I32 currentLevel = _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT);
		if ( currentLevel > maxLevel )
		{
			_parent->GetCounter()->SetValue(GCID_NORMAL, GNORMAL_TOWER_MAX, currentLevel);
			_parent->GetPushCharge()->TryTrigger(PushChargeType_Tower, currentLevel);
			_parent->GetCounter()->SetValueWithActivity(GCID_WARRIOR_SEVEN_IDX1, WARRIOR_DAY5_TOWER_LEVEL, currentLevel);
		}
		srchigh = RDMS_EN_TYPE(RDMS_TOWER);
		
		// 更新跨服计数器,提交世界成就
		GameBigworldCounter::GetInstance()->commitTower(_parent, currentLevel);
	}
	else if (1 == data.type)
	{
		_parent->GetCounter()->IncValue(GCID_UP2400, GUP2400_TOWER_WIN1);
		_parent->GetCounter()->IncValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_1);
		_parent->GetPushCharge()->TryTrigger(PushChargeType_Tower_LC, _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_1));
		srchigh = RDMS_EN_TYPE(RDMS_TOWER_1);
	
		I32 currentLevel = _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_1);
		// 更新跨服计数器,提交世界成就
		GameBigworldCounter::GetInstance()->commitTowerGROUP(_parent, currentLevel, data.type);
	}
	else if (2 == data.type)
	{
		_parent->GetCounter()->IncValue(GCID_UP2400, GUP2400_TOWER_WIN2);
		_parent->GetCounter()->IncValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_2);
		_parent->GetPushCharge()->TryTrigger(PushChargeType_Tower_QW, _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_2));
		srchigh = RDMS_EN_TYPE(RDMS_TOWER_2);
			
		I32 currentLevel = _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_2);
		// 更新跨服计数器,提交世界成就
		GameBigworldCounter::GetInstance()->commitTowerGROUP(_parent, currentLevel, data.type);
	}
	else if (3 == data.type)
	{
		_parent->GetCounter()->IncValue(GCID_UP2400, GUP2400_TOWER_WIN3);
		_parent->GetCounter()->IncValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_3);
		_parent->GetPushCharge()->TryTrigger(PushChargeType_Tower_DY, _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_3));
		srchigh = RDMS_EN_TYPE(RDMS_TOWER_3);
			
		I32 currentLevel = _parent->GetCounter()->GetValue(GCID_NORMAL, GNORMAL_TOWER_CURRENT_GROUP_3);
		// 更新跨服计数器,提交世界成就
		GameBigworldCounter::GetInstance()->commitTowerGROUP(_parent, currentLevel, data.type);
	}
-
	_parent->ProcessRewardId(pCfg->info[data.type]._reward, sdata.reward, srchigh | RDMS_EN_PARAM(data.towerLevel) );
	_parent->SetDirty(GDM_RANK_DIRTY|GDM_CACHE_DIRTY);

	// @note:活动计数器,从战斗胜利次数修改为战斗次数
	_parent->GetCounter()->IncValueWithActivity(GCID_WARRIOR_SEVEN_IDX1, WARRIOR_DAY3_TOWER_WINS_TIMES);
	_parent->SetDirty(GDM_BASIC_DIRTY);
	return 0;
}


I32 GameTower::onClientGetBossInfo(const C2S_TowerGetBossInfoData &data)
{
	CPacket_Safe spacket(MSG_S2C_TowerGetBossInfoRe);
	S2C_TowerGetBossInfoReData sdata;
	sdata.roleId = _parent->GetBasic()->GetRoleId();
	sdata.towerLevel = data.towerLevel;
	sdata.type = data.type;

	I32 ret = 0;
	do {
		ret = CheckParam(data.towerLevel, data.type);
		if (ret)
			break;
		
		// 前面检查过对应配置了
		const TempTowerCfg* pCfg = TEMPMGR->GetOneTowerCfg(data.towerLevel);
		I32 battleId = pCfg->info[data.type]._battleId;
		if (battleId == 0)
		{
			ret = ERR_CLIENT_DATA;
			break;
		}
		
		ret = BATTLEPOOL->Format(battleId, sdata.enemy);
	} while (false);

	sdata.ret = ret;
	sdata.Encode(spacket);
	_parent->SendToGateWay(spacket);
	return ret;
}


I32 GameTower::OnRoleTowerPostChallenge(C2S_TowerPostChallengeData& data)
{
	I32 ret = CheckParamChallenge(data.towerLevel, data.type);
	if (ret)
		return ret;
	
	S2C_TowerPostChallengeReData sdata;

	I32 retForWin = 0;
	if(data.result.rst)
	{ // 检验是否可以战胜
		const TempTowerCfg* pCfg = TEMPMGR->GetOneTowerCfg(data.towerLevel);
		if (NULL == pCfg)
		{
			retForWin = ERR_SERVER_DATA;
		}
		if ( pCfg && !_parent->canFightWin(pCfg->info[data.type]._battleId) )
		{ // 强制判负
			data.result.rst = 0;
			retForWin = ERR_FIGHT_CANNOT_WIN;
		}
	}

	sdata.roleId = data.roleId;
	sdata.towerLevel = data.towerLevel;
	sdata.result = data.result;

	if(retForWin)
		sdata.ret = retForWin;
	else
		sdata.ret = OnTowerPostChallenge(data, sdata);
	
	CPacket_Safe spacket(MSG_S2C_TowerPostChallengeRe);
	sdata.Encode(spacket);

	_parent->SendToGateWay(spacket);

	return sdata.ret;
}

// -- 深渊速通
I32 GameTower::OnRoleTowerFastChallenge(C2S_TowerFastChallengeData& data)
{
	I32 ret = CheckParamChallenge(data.towerLevel, data.type);
	if (ret)
		return ret;
	
	S2C_TowerFastChallengeReData sdata;

	sdata.roleId = data.roleId;
	sdata.type = data.type;
	sdata.towerLevel = data.towerLevel;

	sdata.ret = OnTowerFastChallenge(data, sdata);
	
	CPacket_Safe spacket(MSG_S2C_TowerFastChallengeRe);
	sdata.Encode(spacket);

	_parent->SendToGateWay(spacket);

	return sdata.ret;
}


IMPLEMENT_SVRFUN(MSG_C2S_TowerPostChallenge)
{
	C2S_TowerPostChallengeData data;
	data.Decode(packet);

	I32 ret = ERR_INVALID;

	GameRole *role = GRManager::GetInstance()->GetGameRoleByRid(data.roleId);
	if ( role )
		ret = role->GetTower()->OnRoleTowerPostChallenge(data);
	XLOG_TRACE(ret, &data);
}

// -- 速通
IMPLEMENT_SVRFUN(MSG_C2S_TowerFastChallenge)
{
	C2S_TowerFastChallengeData data;
	data.Decode(packet);

	I32 ret = ERR_INVALID;

	GameRole *role = GRManager::GetInstance()->GetGameRoleByRid(data.roleId);
	if ( role )
		ret = role->GetTower()->OnRoleTowerFastChallenge(data);
	XLOG_TRACE(ret, &data);
}


IMPLEMENT_SVRFUN(MSG_C2S_TowerGetBossInfo)
{
	C2S_TowerGetBossInfoData data;
	data.Decode(packet);

	I32 ret = ERR_INVALID;

	GameRole *role = GRManager::GetInstance()->GetGameRoleByRid(data.roleId);
	if ( role )
		ret = role->GetTower()->onClientGetBossInfo(data);
	XLOG_TRACE(ret, &data);
}

//-------------------------------------------------------------------------------------------------------------------
void GameTower::PreCrossDay(I32 crossCount)
{
	return;
}

