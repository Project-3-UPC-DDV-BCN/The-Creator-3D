﻿using System.Runtime.CompilerServices;

namespace TheEngine.TheGOAPAgent
{
    public class TheGOAPAgent : TheComponent
    {
        public int num_goals
        {
            get
            {
                return GetNumGoals();
            }
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        private extern int GetNumGoals();

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern float GetBlackboardVariableF();

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern bool GetBlackboardVariableB();

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern string GetGoalName(int index);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern string GetGoalConditionName(int index);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern void SetGoalPriority(int index, int priority);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern int GetGoalPriority(int index);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern void CompleteAction();

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern void FailAction();

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern void SetBlackboardVariable(string name, float value);

        [MethodImpl(MethodImplOptions.InternalCall)]
        public extern void SetBlackboardVariable(string name, bool value);
    }
}
